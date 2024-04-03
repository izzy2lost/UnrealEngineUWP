// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class IPersonaPreviewScene;
class UPhysicsControlAsset;
class UPhysicsControlAssetEditorSkeletalMeshComponent;

/**
 * Helper/container for data used by the Physics Control Profile Editor 
 */
class FPhysicsControlAssetEditorData
{
public:
	FPhysicsControlAssetEditorData();

	/** Initializes members */
	void Initialize(const TSharedRef<IPersonaPreviewScene>& InPreviewScene);

	/** Caches a preview mesh. Sets us to a default mesh if none is set yet (or if an older one got deleted) */
	void CachePreviewMesh();

public:
	/** The asset being inspected */
	TObjectPtr<UPhysicsControlAsset> PhysicsControlAsset;

	/** Skeletal mesh component specialized for this asset editor */
	UPhysicsControlAssetEditorSkeletalMeshComponent* EditorSkelComp;

	/** The physics control component used for testing/simulating on the character */
	class UPhysicsControlComponent* PhysicsControlComponent;

	/** Preview scene */
	TWeakPtr<IPersonaPreviewScene> PreviewScene;

};
