// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskSVE.h"

#include "GeometryMaskSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

FGeometryMaskSceneViewExtension::FGeometryMaskSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* InWorld)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
{
}

void FGeometryMaskSceneViewExtension::BeginRenderViewFamily(
	FSceneViewFamily& InViewFamily)
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		Subsystem->Update(GetWorld(), InViewFamily);
	}
}

bool FGeometryMaskSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	bool bIsActive = FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
	if (!bIsActive)
	{
		return false;
	}

#if WITH_EDITOR
	if (GEditor)
	{
		if (GEditor->IsSimulatingInEditor())
		{
			bIsActive = GetWorld()->WorldType == EWorldType::Editor;
		}
		else if (GEditor->PlayWorld)
		{
			bIsActive = GetWorld()->WorldType == EWorldType::PIE;
		}
		else
		{
			bIsActive = GetWorld()->WorldType == EWorldType::Editor;	
		}		
	}
#endif

	return bIsActive;
}
