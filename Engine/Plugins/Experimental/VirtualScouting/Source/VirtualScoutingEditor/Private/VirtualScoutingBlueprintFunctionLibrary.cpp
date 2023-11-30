// Copyright Epic Games, Inc. All Rights Reserved.


#include "VirtualScoutingBlueprintFunctionLibrary.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEdEngine.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"

bool UVirtualScoutingBlueprintFunctionLibrary::CheckIsWithEditor()
{

#if WITH_EDITOR
	return true;
#endif
	
	return false;
}

bool UVirtualScoutingBlueprintFunctionLibrary::DeleteActors(const TArray<AActor*>& InActorsToDelete)
{
	const FScopedTransaction Transaction(NSLOCTEXT("VirtualScouting", "DeleteActors", "Delete Actors"));

	UWorld* World = nullptr;
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	World = UnrealEditorSubsystem->GetEditorWorld();
	if (World)
	{
		// select
		UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
		if (EditorActorSubsystem)
		{
			EditorActorSubsystem->SetSelectedLevelActors(InActorsToDelete);
		}
		// delete selected
		const bool bVerifyDeletionCanHappen = true;
		const bool bWarnAboutReferences = false;
		return GEditor->edactDeleteSelected(World, bVerifyDeletionCanHappen, bWarnAboutReferences, bWarnAboutReferences);
	}

	// If EditorWorld is not valid (i.e. VRPreviewMode), just call DestroyActor
	bool bDeleted = false;
	for (AActor* Actor : InActorsToDelete)
	{
		World = Actor->GetWorld();
		if (World)
		{
			bDeleted |= World->DestroyActor(Actor);
		}
	}
	return bDeleted;
}