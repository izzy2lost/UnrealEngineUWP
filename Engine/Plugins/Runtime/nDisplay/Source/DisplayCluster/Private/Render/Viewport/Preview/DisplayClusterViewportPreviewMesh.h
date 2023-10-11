// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Render/DisplayDevice/Containers/DisplayClusterDisplayDevice_Enums.h"

#include "Misc/DisplayClusterObjectRef.h"

class UDisplayClusterDisplayDeviceBaseComponent;
class FDisplayClusterViewport;

/**
* DCRA has several types of preview meshes.
*/
enum class EDisplayClusterViewportPreviewMeshType : uint8
{
	PreviewMesh = 0,
	PreviewEditableMesh
};

/**
 * Runtime configuration of preview mesh.
 */
enum class EDisplayClusterViewportPreviewMeshFlags : uint8
{
	None = 0,

	// The Mesh component has been removed
	HasDeletedMeshComponent = 1 << 0,

	// the Mesh component has been modified
	HasChangedMeshComponent = 1 << 1,

	// The MaterialInstance has been removed
	HasDeletedMaterialInstance = 1 << 2,

	// The MaterialInstance has been modified
	HasChangedMaterialInstance = 1 << 3,

	// Default Material is restored on the Mesh
	HasRestoredDefaultMaterial = 1 << 4,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportPreviewMeshFlags);

/**
* Manage preview mesh of the viewport
*/
class FDisplayClusterViewportPreviewMesh
{
public:
	FDisplayClusterViewportPreviewMesh(const EDisplayClusterViewportPreviewMeshType InMeshType)
		: MeshType(InMeshType)
	{ }

	~FDisplayClusterViewportPreviewMesh() = default;

public:
	/** Get preview mesh component. */
	UMeshComponent* GetMeshComponent() const;

	/** Get MID used on preview mesh. */
	UMaterialInstanceDynamic* GetMaterialInstance() const;

	EDisplayClusterDisplayDeviceMaterialType GetCurrentMaterialType() const
	{
		return CurrentMaterialType;
	}

	/** Get current material. */
	UMaterial* GetCurrentMaterial() const;

	/** Get default material. */
	UMaterial* GetDefaultMaterial() const;

	/** Update mesh component and materials for viewport. */
	void Update(FDisplayClusterViewport& InViewport, UDisplayClusterDisplayDeviceBaseComponent& InDisplayDeviceComponent);

	/** Restore default material and release mesh component with materials for viewport. */
	void Release(FDisplayClusterViewport& InViewport);

	/** Release mesh component and materials for viewport. */
	void Reset();

	/** Returns true if the runtime flags have any of the input flags. */
	bool HasAnyFlag(const EDisplayClusterViewportPreviewMeshFlags InMeshFlags) const
	{
		return EnumHasAnyFlags(RuntimeFlags, InMeshFlags);
	}

private:
	/** Returns true if this mesh type is supported by the viewport projection policy and DCRA. */
	bool ShouldUseMeshComponent(FDisplayClusterViewport& InViewport) const;

	/** Get mesh component. */
	UMeshComponent* GetOrCreatePreviewMeshComponent(FDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) const;

private:
	// the type of mesh
	const EDisplayClusterViewportPreviewMeshType MeshType;

	// runtime flags
	EDisplayClusterViewportPreviewMeshFlags RuntimeFlags;

	// Mesh component used for preview
	TObjectPtr<UMeshComponent> MeshComponentPtr = nullptr;

	// This mesh component exists in DCRA and does not need to be deleted.
	bool bIsRootActorMeshComponent = false;

	// Preview material used on the mesh
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstancePtr = nullptr;

	// The current material assigned to the preview mesh
	TObjectPtr<UMaterial> CurrentMaterialPtr = nullptr;

	// The default material defined in the DisplayDevice
	TObjectPtr<UMaterial> DefaultMaterialPtr = nullptr;

	// The type of material that is currently used on the mesh
	EDisplayClusterDisplayDeviceMaterialType CurrentMaterialType = EDisplayClusterDisplayDeviceMaterialType::DefaultPreviewMeshMaterial;
};
