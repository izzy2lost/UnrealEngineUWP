// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDTabSpawnerBase.h"

#include "ChaosVDEngine.h"
#include "ChaosVDScene.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDTabSpawnerBase::FChaosVDTabSpawnerBase(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget)
{
	OwningTabWidget = InOwningTabWidget;
	InTabManager->RegisterTabSpawner(InTabID, FOnSpawnTab::CreateRaw(this, &FChaosVDTabSpawnerBase::HandleTabSpawned));
}

void FChaosVDTabSpawnerBase::HandleTabClosed(const TSharedRef<SDockTab>& InTabClosed)
{
	OnTabDestroyed().Broadcast(InTabClosed);
}

TSharedRef<SWidget> FChaosVDTabSpawnerBase::GenerateErrorWidget()
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChaosVDEditorTabSpawner", "Failed to generate Tab content"))
		];
}

UWorld* FChaosVDTabSpawnerBase::GetChaosVDWorld() const
{
	if (TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin())
	{
		return ScenePtr->GetUnderlyingWorld();
	}

	return nullptr;
}

TWeakPtr<FChaosVDScene> FChaosVDTabSpawnerBase::GetChaosVDScene() const
{
	const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin();
	if (ensure(MainTabPtr))
	{
		const TSharedRef<FChaosVDEngine> ChaosVDEngine = MainTabPtr->GetChaosVDEngineInstance();
		if (TSharedPtr<FChaosVDScene> ScenePtr = ChaosVDEngine->GetCurrentScene())
		{
			return ScenePtr;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
