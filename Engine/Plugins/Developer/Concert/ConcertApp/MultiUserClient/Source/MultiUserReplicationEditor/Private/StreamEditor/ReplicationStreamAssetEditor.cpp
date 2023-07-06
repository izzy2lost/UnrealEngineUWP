// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationStreamAssetEditor.h"

#include "ReplicationStreamEditorToolkit.h"

void UReplicationStreamAssetEditor::SetObjectToEdit(UObject* InObject)
{
	ObjectToEdit = InObject;
}

void UReplicationStreamAssetEditor::GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit)
{
	OutObjectsToEdit = { ObjectToEdit };
}

TSharedPtr<FBaseAssetToolkit> UReplicationStreamAssetEditor::CreateToolkit()
{
	return MakeShared<UE::MultiUserReplicationEditor::FReplicationStreamEditorToolkit>(this);
}
