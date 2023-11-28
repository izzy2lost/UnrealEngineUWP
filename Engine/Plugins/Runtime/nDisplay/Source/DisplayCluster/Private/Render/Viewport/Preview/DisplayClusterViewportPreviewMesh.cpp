// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Preview/DisplayClusterViewportPreviewMesh.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/StaticMeshComponent.h"

namespace UE::DisplayCluster::ViewportPreviewMesh
{
	/** Reset the mesh material to default values from its archetype. */
	static inline bool RestoreMeshMaterialsFromArchetype(UMeshComponent* InMeshComponent)
	{
		if (const UMeshComponent* MeshArchetype = Cast<UMeshComponent>(InMeshComponent->GetArchetype()))
		{
			// Retrieve material on the preview mesh from an archetype or OrigMaterial.
			if (UMaterialInterface* OrigMaterial = MeshArchetype->OverrideMaterials.IsValidIndex(0) ? MeshArchetype->OverrideMaterials[0] : nullptr)
			{
				InMeshComponent->SetMaterial(0, OrigMaterial);

				return true;
			}
		}

		return false;
	}

	/** Get material for preview mesh. */
	TObjectPtr<UMaterial> FindPreviewMeshMaterial(const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType, UDisplayClusterDisplayDeviceBaseComponent* InDisplayDeviceComponent, UDisplayClusterCameraComponent* ViewPointComponent)
	{
		TObjectPtr<UMaterial> OutMaterial = nullptr;

		// First get the material from the ViewPoint component (WarpPolicy)
		OutMaterial = ViewPointComponent ? ViewPointComponent->GetDisplayDeviceMaterial(InMeshType, InMaterialType) : nullptr;

		// Finally, get the material from the DisplayDevice
		if (!OutMaterial)
		{
			OutMaterial = InDisplayDeviceComponent ? InDisplayDeviceComponent->GetDisplayDeviceMaterial(InMeshType, InMaterialType) : nullptr;
		}

		// Ignore deleted materials
		return OutMaterial;
	}
};

////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportPreviewMesh
////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterDisplayDeviceMaterialType FDisplayClusterViewportPreviewMesh::GetCurrentMaterialType() const
{
	return Configuration->IsTechvisEnabled()
		? EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial
		: EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial;
}

void FDisplayClusterViewportPreviewMesh::Update(FDisplayClusterViewport* InViewport, UDisplayClusterDisplayDeviceBaseComponent* InDisplayDeviceComponent, UDisplayClusterCameraComponent* ViewPointComponent)
{
	using namespace UE::DisplayCluster::ViewportPreviewMesh;

	// Reset runtime flags before each update
	RuntimeFlags = EDisplayClusterViewportPreviewMeshFlags::None;

	if (!InViewport || !InViewport->ViewportPreview->HasAnyFlags(EDisplayClusterViewportPreviewFlags::HasValidPreviewRTT) || !ShouldUseMeshComponent(InViewport) || !InDisplayDeviceComponent)
	{
		// The mesh component and its resources are no longer used.
		Release(InViewport);

		return;
	}

	// Update default material
	DefaultMaterialPtr = FindPreviewMeshMaterial(EDisplayClusterDisplayDeviceMeshType::DefaultMesh, GetCurrentMaterialType(), InDisplayDeviceComponent, ViewPointComponent);

	// Get current preview material
	UMaterial* InMeshMaterial = FindPreviewMeshMaterial(MeshType, GetCurrentMaterialType(), InDisplayDeviceComponent, ViewPointComponent);
	if (!InMeshMaterial)
	{
		// Do not use preview if material is not defined.
		Release(InViewport);

		return;
	}

	// Get or create warp mesh:
	bool bNewIsRootActorPreviewMesh = false;
	UMeshComponent* NewMeshComponent = GetOrCreatePreviewMeshComponent(InViewport, bNewIsRootActorPreviewMesh);
	if (GetMeshComponent() != NewMeshComponent || bNewIsRootActorPreviewMesh != bIsRootActorMeshComponent)
	{
		// Release the reference to the old mesh component
		Release(InViewport);
	}
	else if (CurrentMaterialPtr != InMeshMaterial)
	{
		// Release a material instance when the material is changed
		ReleaseMaterialInstance();
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
		// Update material instance and assign to the  mesh
		if (GetMaterialInstance() == nullptr)
		{
			CurrentMaterialPtr = InMeshMaterial;

			MaterialInstancePtr = UMaterialInstanceDynamic::Create(InMeshMaterial, MeshComponent);
			EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasChangedMaterialInstance);
		}

		// The material must be assigned to the mesh at each tick, without any conditions in case it can be changed externally.
		MeshComponent->SetMaterial(0, GetMaterialInstance());
	}

	// Handling the material overlay logic for the preview mesh:
	UpdateOverlayMaterial(InViewport);
}

void FDisplayClusterViewportPreviewMesh::AddReferencedObjects(FReferenceCollector& Collector)
{
	TObjectPtr<UObject> OwnedObjects[] = { MeshComponentPtr, OrigOverlayMaterial, MaterialInstancePtr, CurrentMaterialPtr, DefaultMaterialPtr };

	for (TObjectPtr<UObject>& OwnedObject : OwnedObjects)
	{
		if (OwnedObject)
		{
			Collector.AddReferencedObject(OwnedObject);
		}
	}
}

void FDisplayClusterViewportPreviewMesh::SetCustomOverlayMaterial(UMeshComponent* InMeshComponent, UMaterialInterface* InOverlayMaterial)
{
	if (InMeshComponent)
	{
		if (OrigOverlayMaterial == nullptr)
		{
			OrigOverlayMaterial = InMeshComponent->GetOverlayMaterial();
		}

		InMeshComponent->SetOverlayMaterial(InOverlayMaterial);
	}
}

void FDisplayClusterViewportPreviewMesh::RestoreOverlayMaterial(UMeshComponent* InMeshComponent)
{
	if (InMeshComponent)
	{
		if (OrigOverlayMaterial)
		{
			InMeshComponent->SetOverlayMaterial(OrigOverlayMaterial);
		}
		else
		{
			if (const UMeshComponent* MeshArchetype = Cast<UMeshComponent>(InMeshComponent->GetArchetype()))
			{
				// Retrieve material on the preview mesh from an archetype or OrigMaterial.
				if (UMaterialInterface* ArchetypeOverlayMaterial = MeshArchetype->GetOverlayMaterial())
				{
					InMeshComponent->SetOverlayMaterial(ArchetypeOverlayMaterial);
				}
			}
		}
	}
}

void FDisplayClusterViewportPreviewMesh::UpdateOverlayMaterial(FDisplayClusterViewport* InViewport)
{
	// Update material instance and assign to the  mesh
	if (UMeshComponent* MeshComponent = GetMeshComponent())
	{
		if (Configuration->GetPreviewSettings().bPreviewEnableOverlayMaterial)
		{
			// Restore overlay material
			RestoreOverlayMaterial(MeshComponent);
		}
		else
		{
			// Disable overlay material when preview is used
			SetCustomOverlayMaterial(MeshComponent, nullptr);
		}
	}
}

void FDisplayClusterViewportPreviewMesh::ReleaseMeshComponent(FDisplayClusterViewport* InViewport)
{
	using namespace UE::DisplayCluster::ViewportPreviewMesh;

	UMeshComponent* MeshComponent = GetMeshComponent();
	if (!MeshComponent && MeshComponentPtr)
	{
		// Restore materials on deleted mesh
		RestoreOverlayMaterial(MeshComponentPtr);
		RestoreMeshMaterialsFromArchetype(MeshComponentPtr);

		// The mesh was destroyed earlier, (re-running build scripts inside RootActor), but we need to update the new mesh component to.
		MeshComponent = GetOrCreatePreviewMeshComponent(InViewport, bIsRootActorMeshComponent);
	}

	if (MeshComponent)
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasDeletedMeshComponent);

		// Restore materials on exists mesh
		RestoreOverlayMaterial(MeshComponent);
		if (!RestoreMeshMaterialsFromArchetype(MeshComponent) && DefaultMaterialPtr)
		{
			MeshComponent->SetMaterial(0, DefaultMaterialPtr);
		}

		if (!bIsRootActorMeshComponent)
		{
			// Release this mesh component from DCRA
			MeshComponent->UnregisterComponent();
			MeshComponent->DestroyComponent();
		}
		else
		{
			// Restore the default material for an existing mesh in DCRA
			EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewMeshFlags::HasRestoredDefaultMaterial);
		}
	}

	MeshComponentPtr = nullptr;
}

void FDisplayClusterViewportPreviewMesh::ReleaseMaterialInstance()
{
	// The material instance references the mesh, so it must also be deleted
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
	CurrentMaterialPtr = nullptr;
}

bool FDisplayClusterViewportPreviewMesh::ShouldUseMeshComponent(FDisplayClusterViewport* InViewport) const
{
	if (!InViewport || !InViewport->GetRenderSettings().bEnable)
	{
		// disable viewport
		return false;
	}

	const FDisplayClusterViewport_PreviewSettings PreviewSettings = Configuration->GetRenderFrameSettings().PreviewSettings;
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy = InViewport->GetProjectionPolicy();
	if (ProjectionPolicy.IsValid())
	{
		switch (MeshType)
		{
		case EDisplayClusterDisplayDeviceMeshType::PreviewMesh:
			return ProjectionPolicy->HasPreviewMesh(InViewport) && PreviewSettings.bEnablePreviewMesh;

		case EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh:
			return ProjectionPolicy->HasPreviewEditableMesh(InViewport) && PreviewSettings.bEnablePreviewEditableMesh;

		default:
			break;
		}
	}

	return false;
}

UMeshComponent* FDisplayClusterViewportPreviewMesh::GetOrCreatePreviewMeshComponent(FDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) const
{
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy = InViewport ? InViewport->GetProjectionPolicy() : nullptr;
	if (ProjectionPolicy.IsValid())
	{
		switch (MeshType)
		{
		case EDisplayClusterDisplayDeviceMeshType::PreviewMesh:
			return ProjectionPolicy->GetOrCreatePreviewMeshComponent(InViewport, bOutIsRootActorComponent);

		case EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh:
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
	return MeshComponentPtr;
}

UMaterialInstanceDynamic* FDisplayClusterViewportPreviewMesh::GetMaterialInstance() const
{
	return MaterialInstancePtr;
}
