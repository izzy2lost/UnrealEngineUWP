// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileAsset.h"
#include "Engine/SkeletalMesh.h"

//======================================================================================================================
UPhysicsControlProfileAsset::UPhysicsControlProfileAsset()
{
}

//======================================================================================================================
void UPhysicsControlProfileAsset::Log()
{
	UE_LOG(LogTemp, Log, TEXT("Number of initial controls = %d"), InitialControls.Num());
	UE_LOG(LogTemp, Log, TEXT("Number of initial body modifiers = %d"), InitialBodyModifiers.Num());
}

//======================================================================================================================
void UPhysicsControlProfileAsset::MakeControlsAndModifiersFromCharacterSetupData()
{
}

#if WITH_EDITOR
//======================================================================================================================
const FName UPhysicsControlProfileAsset::GetPreviewMeshPropertyName()
{
	return GET_MEMBER_NAME_STRING_CHECKED(UPhysicsControlProfileAsset, PreviewSkeletalMesh);
};
#endif

//======================================================================================================================
void UPhysicsControlProfileAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty)
{
	PreviewSkeletalMesh = PreviewMesh;
}

//======================================================================================================================
USkeletalMesh* UPhysicsControlProfileAsset::GetPreviewMesh() const
{
	return PreviewSkeletalMesh.LoadSynchronous();
}
