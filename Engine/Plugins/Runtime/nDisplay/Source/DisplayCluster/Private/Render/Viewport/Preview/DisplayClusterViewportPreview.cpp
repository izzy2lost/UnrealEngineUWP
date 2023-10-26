// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"
#include "Render/Viewport/Resource/DisplayClusterViewportResource.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Warp/IDisplayClusterWarpPolicy.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"

#include "Components/StaticMeshComponent.h"

////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportPreview
////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportPreview::FDisplayClusterViewportPreview(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId)
	: Configuration(InConfiguration)
	, ViewportId(InViewportId)
	, ClusterNodeId(InConfiguration->GetClusterNodeId())
	, PreviewMesh(EDisplayClusterViewportPreviewMeshType::PreviewMesh, InConfiguration)
	, PreviewEditableMesh(EDisplayClusterViewportPreviewMeshType::PreviewEditableMesh, InConfiguration)
{ }

FDisplayClusterViewportPreview::~FDisplayClusterViewportPreview()
{
	Release();
}

void FDisplayClusterViewportPreview::Initialize(FDisplayClusterViewport& InViewport)
{
	ViewportWeakPtr = InViewport.AsShared();
}

void FDisplayClusterViewportPreview::Release()
{
	PreviewRTT.Reset();
	RuntimeFlags = EDisplayClusterViewportPreviewFlags::None;

	FDisplayClusterViewport* InViewport = GetViewportImpl();
	PreviewMesh.Release(InViewport);
	PreviewEditableMesh.Release(InViewport);
	}

void FDisplayClusterViewportPreview::Update()
{
	RuntimeFlags = EDisplayClusterViewportPreviewFlags::None;

	// Update viewport output RTT
	TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> NewPreviewRTT = GetOutputPreviewTargetableResources();
	if (NewPreviewRTT != PreviewRTT)
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasChangedPreviewRTT);
		PreviewRTT = NewPreviewRTT;
	}

	FDisplayClusterViewport* InViewport = GetViewportImpl();
	UDisplayClusterDisplayDeviceBaseComponent* InDisplayDeviceComponent = InViewport ? InViewport->GetDisplayDeviceComponent(EDisplayClusterRootActorType::Configuration) : nullptr;

	// Update preview meshes only if DisplayDevice is used
	PreviewMesh.Update(InViewport, InDisplayDeviceComponent);
	PreviewEditableMesh.Update(InViewport, InDisplayDeviceComponent);

	// Update Runtime Flags:
	if (PreviewMesh.HasAnyFlag(EDisplayClusterViewportPreviewMeshFlags::HasDeletedMaterialInstance | EDisplayClusterViewportPreviewMeshFlags::HasChangedMaterialInstance))
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasChangedPreviewMeshMaterialInstance);
	}
	if (PreviewEditableMesh.HasAnyFlag(EDisplayClusterViewportPreviewMeshFlags::HasDeletedMaterialInstance | EDisplayClusterViewportPreviewMeshFlags::HasChangedMaterialInstance))
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasChangedPreviewEditableMeshMaterialInstance);
	}

	// Update the preview mesh and materials in the DisplayDevice component
	if (InDisplayDeviceComponent)
	{
		// Update material instances
		InDisplayDeviceComponent->OnUpdateDisplayDeviceMaterialInstance(*this, EDisplayClusterDisplayDeviceMeshType::PreviewMesh, PreviewMesh.GetCurrentMaterialType(), PreviewMesh.GetMaterialInstance());
		InDisplayDeviceComponent->OnUpdateDisplayDeviceMaterialInstance(*this, EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh, PreviewMesh.GetCurrentMaterialType(), PreviewEditableMesh.GetMaterialInstance());

		// Update mesh components
		InDisplayDeviceComponent->OnUpdateDisplayDeviceMeshComponent(*this, EDisplayClusterDisplayDeviceMeshType::PreviewMesh, PreviewMesh.GetMeshComponent());
		InDisplayDeviceComponent->OnUpdateDisplayDeviceMeshComponent(*this, EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh, PreviewEditableMesh.GetMeshComponent());
	}

	// The warp policy can update editable meshes and their material parameters
	if (InViewport && InViewport->GetProjectionPolicy().IsValid())
	{
		if (IDisplayClusterWarpPolicy* WarpPolicy = InViewport->GetProjectionPolicy()->GetWarpPolicy())
		{
			WarpPolicy->OnUpdatePreviewEditableMesh(*this, PreviewEditableMesh.GetMeshComponent(), PreviewEditableMesh.GetCurrentMaterialType(), PreviewEditableMesh.GetMaterialInstance());
		}
	}
}

IDisplayClusterViewport* FDisplayClusterViewportPreview::GetViewport() const
{
	return GetViewportImpl();
}

FDisplayClusterViewport* FDisplayClusterViewportPreview::GetViewportImpl() const
{
	return ViewportWeakPtr.IsValid() ? ViewportWeakPtr.Pin().Get() : nullptr;
}

TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> FDisplayClusterViewportPreview::GetOutputPreviewTargetableResources() const
{
	if (FDisplayClusterViewport* InViewport = GetViewportImpl())
	{
		const TArray<TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>>& PreviewResources = InViewport->GetViewportResources(EDisplayClusterViewportResource::OutputPreviewTargetableResources);
		if (!PreviewResources.IsEmpty())
		{
			return PreviewResources[0];
		}
	}

	return nullptr;
}

UTextureRenderTarget2D* FDisplayClusterViewportPreview::GetPreviewTextureRenderTarget2D() const
{
	if (PreviewRTT.IsValid())
	{
		return PreviewRTT->GetTextureRenderTarget2D();
	}

	return nullptr;
}
