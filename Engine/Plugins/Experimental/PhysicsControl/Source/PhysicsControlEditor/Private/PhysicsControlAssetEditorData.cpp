// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetEditorData.h"
#include "PhysicsControlAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "PhysicsControlAssetEditorData"

//======================================================================================================================
FPhysicsControlAssetEditorData::FPhysicsControlAssetEditorData()
{
}

//======================================================================================================================
void FPhysicsControlAssetEditorData::Initialize(const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	PreviewScene = InPreviewScene;

	EditorSkelComp = nullptr;
	PhysicsControlComponent = nullptr;
	FSoftObjectPath PreviewMeshStringRef = PhysicsControlAsset->PreviewSkeletalMesh.ToSoftObjectPath();

	// Support undo/redo
	PhysicsControlAsset->SetFlags(RF_Transactional);
}

//======================================================================================================================
void FPhysicsControlAssetEditorData::CachePreviewMesh()
{
	USkeletalMesh* PreviewMesh = PhysicsControlAsset->PreviewSkeletalMesh.LoadSynchronous();

	if (PreviewMesh == nullptr)
	{
		// Fall back to the default skeletal mesh in the EngineMeshes package.
		// This is statically loaded as the package is likely not fully loaded
		// (otherwise, it would have been found in the above iteration).
		PreviewMesh = (USkeletalMesh*)StaticLoadObject(USkeletalMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"), NULL, LOAD_None, NULL);
		check(PreviewMesh);

		PhysicsControlAsset->PreviewSkeletalMesh = PreviewMesh;

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("Error_PhysicsControlAssetHasNoSkelMesh", "Warning: Physics Control Profile Asset has no skeletal mesh assigned.\nFor now, a simple default skeletal mesh ({0}) will be used.\nYou can fix this by opening the asset and choosing another skeletal mesh from the toolbar."),
			FText::FromString(PreviewMesh->GetFullName())));
	}
	else if (PreviewMesh->GetSkeleton() == nullptr)
	{
		// Fall back in the case of a deleted skeleton
		PreviewMesh = (USkeletalMesh*)StaticLoadObject(USkeletalMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"), NULL, LOAD_None, NULL);
		check(PreviewMesh);

		PhysicsControlAsset->PreviewSkeletalMesh = PreviewMesh;

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("Error_PhysicsControlAssetHasNoSkelMeshSkeleton", "Warning: Physics Control Profile Asset has a skeletal mesh with no skeleton assigned.\nFor now, a simple default skeletal mesh ({0}) will be used.\nYou can fix this by opening the asset and choosing another skeletal mesh from the toolbar, or repairing the skeleton."),
			FText::FromString(PreviewMesh->GetFullName())));
	}
}


#undef LOCTEXT_NAMESPACE
