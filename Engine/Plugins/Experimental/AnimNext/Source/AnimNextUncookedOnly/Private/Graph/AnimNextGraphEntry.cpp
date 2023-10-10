// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphEntry.h"
#include "Graph/AnimNextGraph_EditorData.h"

void UAnimNextGraphEntry::Initialize(UAnimNextGraph_EditorData* InEditorData)
{
	InEditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	InEditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextGraphEntry::HandleRigVMGraphModifiedEvent);
}

void UAnimNextGraphEntry::HandleRigVMGraphModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	
}

bool UAnimNextGraphEntry::IsAsset() const
{
	// Entries are considered assets to allow using the asset logic for save dialogs, etc.
	// Also, they return true even if pending kill, in order to show up as deleted in these dialogs.
	return IsPackageExternal() && !GetPackage()->HasAnyFlags(RF_Transient) && !HasAnyFlags(RF_Transient | RF_ClassDefaultObject);
}

FText UAnimNextGraphEntry::GetDisplayName() const
{
	return FText::FromName(GraphName);
}

FText UAnimNextGraphEntry::GetDisplayNameTooltip() const
{
	return FText::FromName(GraphName);
}

void UAnimNextGraphEntry::BroadcastModified()
{
	if(UAnimNextGraph_EditorData* EditorData = Cast<UAnimNextGraph_EditorData>(GetOuter()))
	{
		EditorData->BroadcastModified();
	}
}