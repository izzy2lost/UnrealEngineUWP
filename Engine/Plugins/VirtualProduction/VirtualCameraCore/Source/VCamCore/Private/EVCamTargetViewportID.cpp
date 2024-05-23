// Copyright Epic Games, Inc. All Rights Reserved.

#include "EVCamTargetViewportID.h"

#if WITH_EDITOR
#include "Containers/UnrealString.h"
#include "Engine/Engine.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"

namespace UE::VCamCore
{
	namespace Private
	{
		static FString GetBaseConfigKeyFor(EVCamTargetViewportID TargetViewport)
		{
			/*
			 * Here are example strings for EVCamTargetViewportID == 1: 
			 * One pane: OnePane.Viewport 1.Viewport0
			 * Two pane:
			 *	- Viewport 1.Viewport0
			 *	- Viewport 1.Viewport1
			 * Three pane:
			 *	- ThreePanesLeft.Viewport 1.Viewport0
			 *	- ThreePanesLeft.Viewport 1.Viewport1
			 *	- ThreePanesLeft.Viewport 1.Viewport2
			 * Four pane:
			 *	- FourPanes2x2.Viewport 1.Viewport0
			 *	- FourPanes2x2.Viewport 1.Viewport1
			 *	- FourPanes2x2.Viewport 1.Viewport2
			 *	- FourPanes2x2.Viewport 1.Viewport3
			 */
			return FString::Printf(TEXT("Viewport %d.Viewport"), static_cast<int32>(TargetViewport) + 1);
		}
	}
	
	TSharedPtr<SLevelViewport> GetLevelViewport(EVCamTargetViewportID TargetViewport)
	{
		if (!GEditor)
		{
			return nullptr;
		}
		
		// We consider all layouts that in perspective mode.
		// However, there can be multiple candidates, e.g. in 2x2 layout:
		//	- in the top-right, there is a button for maximizing.
		//	- in the top-left, you can set the mode to "Perspective"
		//	- in the top-left, you can make the viewport immersive (i.e. take up entire screen)
		// We'll just pick one randomly, but we'll favour whatever viewport takes up the most space (immersive > maximized > rest).
		TSharedPtr<SLevelViewport> BestGuess = nullptr;
		
		for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
		{
			TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(Client->GetEditorViewportWidget());
			// E.g. in 2x2 layout you can have several modes, like "Top", "Left". We only care for the "Perspective" mode.
			if (Client->IsOrtho()
				|| !LevelViewport.IsValid())
			{
				continue;
			}
		
			const FString WantedViewportString = Private::GetBaseConfigKeyFor(TargetViewport);
			const FString ViewportConfigKey = LevelViewport->GetConfigKey().ToString();
			const bool bIsInTargetViewport = ViewportConfigKey.Contains(*WantedViewportString, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			if (!bIsInTargetViewport)
			{
				continue;
			}

			// E.g. in a 2x2 layout, there are 4 slots (each one is a SLevelViewport instance).
			// We'll favour whatever viewport takes up the most space (immersive > maximized > rest).
			if (LevelViewport->IsImmersive())
			{
				return LevelViewport;
			}
			if (LevelViewport->IsMaximized())
			{
				BestGuess = LevelViewport;
			}
			else if (!BestGuess)
			{
				BestGuess = LevelViewport;
			}
		}

		return BestGuess;
	}
}
#endif