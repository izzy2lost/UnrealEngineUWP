// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Preview/DisplayClusterViewportPreviewMesh.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Components/StaticMeshComponent.h"

namespace UE::DisplayCluster::Preview
{
	template<class T>
	static inline T* GetObjectProperty(const TObjectPtr<T>& InProperty)
	{
		if (InProperty == nullptr
			|| InProperty->GetName().Find(TEXT("TRASH_")) != INDEX_NONE
			|| InProperty->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
		{
			return nullptr;
		}

		return InProperty;
	}
}
////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportPreviewMesh
////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewportPreviewMesh::Update(FDisplayClusterViewport& InViewport, UDisplayClusterDisplayDeviceBaseComponent& InDisplayDeviceComponent)
{
	// Update default material
	DefaultMaterialPtr = InDisplayDeviceComponent.GetDisplayDeviceMaterial(EDisplayClusterDisplayDeviceMaterialType::DefaultPreviewMeshMaterial);

	// Get current preview material
	EDisplayClusterDisplayDeviceMaterialType NewCurrentMaterialType = InViewport.GetConfiguration().IsTechvisEnabled()
		? EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial
		: EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial;

	UMaterial* InMeshMaterial = InDisplayDeviceComponent.GetDisplayDeviceMaterial(NewCurrentMaterialType);

	// Reset runtime flags before each update
	RuntimeFlags = EDisplayClusterViewportPreviewMeshFlags::None;

	if (!ShouldUseMeshComponent(InViewport) || !InMeshMaterial)
	{
		// The mesh component and its resources are no longer used.
		Release();
		return;
	}

	// Get or create warp mesh:
	bool bNewIsRootActorPreviewMesh = false;
	UMeshComponent* NewMeshComponent = GetOrCreatePreviewMeshComponent(&InViewport, bNewIsRootActorPreviewMesh);
	if (GetMeshComponent() != NewMeshComponent || bNewIsRootActorPreviewMesh != bIsRootActorMeshComponent)
	{
		// Release the reference to the old mesh component
		ReleaseMeshComponent();
	}

	if (NewMeshComponent != GetMeshComponent())
	{
		// The mesh component has been modified.
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasChangedMeshComponent);

		// Update mesh component refs
		bIsRootActorMeshComponent = bNewIsRootActorPreviewMesh;
		MeshComponentPtr = NewMeshComponent;
	}

	// Update material instance and assign to the  mesh
	if (UMeshComponent* MeshComponent = GetMeshComponent())
	{
		if (GetCurrentMaterial() != InMeshMaterial || GetMaterialInstance() == nullptr)
		{
			CurrentMaterialPtr = InMeshMaterial;
			CurrentMaterialType = NewCurrentMaterialType;

			MaterialInstancePtr = UMaterialInstanceDynamic::Create(InMeshMaterial, MeshComponent);
			EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasChangedMaterialInstance);

			// Set preview material
			MeshComponent->SetMaterial(0, GetMaterialInstance());
		}
	}
}

void FDisplayClusterViewportPreviewMesh::ReleaseMeshComponent()
{
	if (UMeshComponent* MeshComponent = GetMeshComponent())
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasDeletedMeshComponent);

		if (!bIsRootActorMeshComponent)
		{
			// Release this mesh component from DCRA
			MeshComponent->UnregisterComponent();
			MeshComponent->DestroyComponent();
		}
		else if (UMaterial* DefaultMaterial = GetDefaultMaterial())
		{
			// Restore the default material for an existing mesh in DCRA
			EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasRestoredDefaultMaterial);

			CurrentMaterialPtr = DefaultMaterial;
			MeshComponent->SetMaterial(0, DefaultMaterial);
		}
	}

	// Remove mesh
	MeshComponentPtr = nullptr;
	CurrentMaterialType = EDisplayClusterDisplayDeviceMaterialType::DefaultPreviewMeshMaterial;

	// The material instance references the mesh, so it must also be deleted
	ReleaseMaterialInstance();
}

void FDisplayClusterViewportPreviewMesh::ReleaseMaterialInstance()
{
	if (UMaterialInstanceDynamic* MaterialInstance = GetMaterialInstance())
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasDeletedMaterialInstance);

		// Clear the material parameters to ensure that no pointers to the texture resources are being kept around
		// Materials destroy their resources in BeginDestroy, so only clear the parameters BeginDestroy hasn't already been called
		if (!MaterialInstance->HasAnyFlags(RF_BeginDestroyed))
		{
			MaterialInstance->ClearParameterValues();
		}
	}

	MaterialInstancePtr = nullptr;
}

bool FDisplayClusterViewportPreviewMesh::ShouldUseMeshComponent(FDisplayClusterViewport& InViewport) const
{
	if (!InViewport.GetRenderSettings().bEnable)
	{
		// disable viewport
		return false;
	}

	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy = InViewport.GetProjectionPolicy();
	if (ProjectionPolicy.IsValid())
	{
		switch (MeshType)
		{
		case EDisplayClusterViewportPreviewMeshType::PreviewMesh:
			return ProjectionPolicy->HasPreviewMesh(&InViewport) && InViewport.Configuration->GetRenderFrameSettings().PreviewSettings.bEnablePreviewMesh;

		case EDisplayClusterViewportPreviewMeshType::PreviewEditableMesh:
			return ProjectionPolicy->HasPreviewEditableMesh(&InViewport) && InViewport.Configuration->GetRenderFrameSettings().PreviewSettings.bEnablePreviewEditableMesh;

		default:
			break;
		}
	}

	return false;
}

UMeshComponent* FDisplayClusterViewportPreviewMesh::GetOrCreatePreviewMeshComponent(FDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) const
{
	check(InViewport);

	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy = InViewport->GetProjectionPolicy();
	if (ProjectionPolicy.IsValid())
	{
		switch (MeshType)
		{
		case EDisplayClusterViewportPreviewMeshType::PreviewMesh:
			return ProjectionPolicy->GetOrCreatePreviewMeshComponent(InViewport, bOutIsRootActorComponent);

		case EDisplayClusterViewportPreviewMeshType::PreviewEditableMesh:
			bOutIsRootActorComponent = false;
			return ProjectionPolicy->GetOrCreatePreviewEditableMeshComponent(InViewport);

		default:
			break;
		}
	}

	return nullptr;
}

UMeshComponent* FDisplayClusterViewportPreviewMesh::GetMeshComponent() const
{
	return UE::DisplayCluster::Preview::GetObjectProperty(MeshComponentPtr);
}

UMaterialInstanceDynamic* FDisplayClusterViewportPreviewMesh::GetMaterialInstance() const
{
	return UE::DisplayCluster::Preview::GetObjectProperty(MaterialInstancePtr);
}

UMaterial* FDisplayClusterViewportPreviewMesh::GetCurrentMaterial() const
{
	return UE::DisplayCluster::Preview::GetObjectProperty(CurrentMaterialPtr);
}

UMaterial* FDisplayClusterViewportPreviewMesh::GetDefaultMaterial() const
{
	return UE::DisplayCluster::Preview::GetObjectProperty(DefaultMaterialPtr);
}
