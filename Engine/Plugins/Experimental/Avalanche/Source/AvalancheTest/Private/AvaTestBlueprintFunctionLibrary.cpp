// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTestBlueprintFunctionLibrary.h"

#include "AvaBlueprint.h"

#if WITH_EDITOR
#include "AvaEditorFunctionLibrary.h"
#endif

void UAvaTestBlueprintFunctionLibrary::ExportAvaBlueprintsToWorld(const TArray<TSoftObjectPtr<UAvalancheBlueprint>>& InBlueprints)
{
#if WITH_EDITOR
	TArray<TWeakObjectPtr<UAvalancheBlueprint>> WeakBlueprints;
	Algo::TransformIf(
		InBlueprints,
		WeakBlueprints,
		[](const TSoftObjectPtr<UAvalancheBlueprint>& InBlueprint)
		{
			return InBlueprint.IsValid() || InBlueprint.LoadSynchronous() != nullptr;
		},
		[](const TSoftObjectPtr<UAvalancheBlueprint>& InBlueprint)
		{
			return MakeWeakObjectPtr(InBlueprint.Get());			
		});
	
	FAvaEditorFunctionLibrary::ExportAvaBlueprintsToWorld(WeakBlueprints);
#else
	UE_LOG(LogAvaTest, Error, TEXT("ExportAvaBlueprintsToWorld is editor-only"));
#endif
}
