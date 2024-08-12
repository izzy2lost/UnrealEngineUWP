// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEntryAssetDefinitions.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

FText UAssetDefinition_AnimNextVariableEntry::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

FText UAssetDefinition_AnimNextAnimationGraphEntry::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

FText UAssetDefinition_AnimNextEventGraphEntry::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

#undef LOCTEXT_NAMESPACE