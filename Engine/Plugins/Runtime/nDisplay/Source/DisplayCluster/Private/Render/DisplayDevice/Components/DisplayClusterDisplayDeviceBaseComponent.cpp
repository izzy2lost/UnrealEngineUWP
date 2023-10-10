// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"
#include "Render/DisplayDevice/DisplayClusterDisplayDeviceStrings.h"

#include "DisplayClusterRootActor.h"

#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/TextureRenderTarget2D.h"

bool UDisplayClusterDisplayDeviceBaseComponent::ShouldUseDisplayDevice(IDisplayClusterViewportConfiguration& InConfiguration) const
{
	// Use this Display device only for preview
	if (InConfiguration.IsPreviewRendering())
	{
		return true;
	}

	return false;
}

TSharedPtr<IDisplayClusterDisplayDeviceProxy, ESPMode::ThreadSafe> UDisplayClusterDisplayDeviceBaseComponent::GetDisplayDeviceProxy(IDisplayClusterViewportConfiguration& InConfiguration)
{
	if (!ShouldUseDisplayDevice(InConfiguration))
	{
		return nullptr;
	}

	UpdateDisplayDeviceProxyImpl(InConfiguration);

	return DisplayDeviceProxy;
}

UDisplayClusterDisplayDeviceBaseComponent::UDisplayClusterDisplayDeviceBaseComponent()
{
	static ConstructorHelpers::FObjectFinder<UMaterial> MeshMaterialObj(UE::DisplayClusterDisplayDeviceStrings::material::asset::mesh);
	check(MeshMaterialObj.Object);
	MeshMaterial = MeshMaterialObj.Object;

	static ConstructorHelpers::FObjectFinder<UMaterial> PreviewMeshMaterialObj(UE::DisplayClusterDisplayDeviceStrings::material::asset::preview_mesh);
	check(PreviewMeshMaterialObj.Object);
	PreviewMeshMaterial = PreviewMeshMaterialObj.Object;

	static ConstructorHelpers::FObjectFinder<UMaterial> PreviewMeshTechvisMaterialObj(UE::DisplayClusterDisplayDeviceStrings::material::asset::preview_techvis_mesh);
	check(PreviewMeshTechvisMaterialObj.Object);
	PreviewMeshTechvisMaterial = PreviewMeshTechvisMaterialObj.Object;
}

TObjectPtr<UMaterial> UDisplayClusterDisplayDeviceBaseComponent::GetDisplayDeviceMaterial(const EDisplayClusterDisplayDeviceMaterialType InMaterialType) const
{
	switch (InMaterialType)
	{
	case EDisplayClusterDisplayDeviceMaterialType::DefaultPreviewMeshMaterial:
		return MeshMaterial;

	case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial:
		return PreviewMeshMaterial;

	case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial:
		return PreviewMeshTechvisMaterial;

	default:
		break;
	}

	return nullptr;
}

void UDisplayClusterDisplayDeviceBaseComponent::OnUpdateDisplayDeviceMaterialInstance(IDisplayClusterViewportPreview& InViewportPreview, const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType, UMaterialInstanceDynamic* InMaterialInstance) const
{
	if (!InMaterialInstance || !ShouldUseDisplayDevice(InViewportPreview.GetConfiguration()))
	{
		return;
	}

	// Update preview RTT parameter values:
	switch (InMaterialType)
	{
	case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial:
	case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial:
		if (InViewportPreview.HasAnyFlags(EDisplayClusterViewportPreviewFlags::HasChangedPreviewMeshMaterialInstance | EDisplayClusterViewportPreviewFlags::HasChangedPreviewEditableMeshMaterialInstance | EDisplayClusterViewportPreviewFlags::HasChangedPreviewRTT))
		{
			// Updates the RTT parameter for the material instance when the PreviewTexture or mesh material is changed.
			InMaterialInstance->SetTextureParameterValue(UE::DisplayClusterDisplayDeviceStrings::material::attr::Preview, InViewportPreview.GetPreviewTextureRenderTarget2D());
		}
		break;
	default:
		break;
	}
}

void UDisplayClusterDisplayDeviceBaseComponent::OnUpdateDisplayDeviceMeshComponent(IDisplayClusterViewportPreview& InViewportPreview, const EDisplayClusterDisplayDeviceMeshType InMeshType, UMeshComponent* InMeshComponent) const
{
	if (!InMeshComponent || !ShouldUseDisplayDevice(InViewportPreview.GetConfiguration()))
	{
		return;
	}

	// Only some of the preview meshes are supported by Techvis
	switch (InMeshType)
	{
		case EDisplayClusterDisplayDeviceMeshType::PreviewMesh:
		case EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh:
			break;

		default:
			// Dont apply Techvis for other mesh types
			return;
	}

	if (InViewportPreview.GetConfiguration().IsTechvisEnabled())
	{
		InMeshComponent->bAffectDynamicIndirectLighting = true;
		InMeshComponent->bAffectIndirectLightingWhileHidden = true;
	}
	else
	{
		// When disabling just revert to the archetype values. The original values at the time of enabling
		// techvis aren't saved, and techvis is enabled by default so they'll always be set to true on new meshes.
		if (const UMeshComponent* MeshArchetype = Cast<UMeshComponent>(InMeshComponent->GetArchetype()))
		{
			InMeshComponent->bAffectDynamicIndirectLighting = MeshArchetype->bAffectDynamicIndirectLighting;
			InMeshComponent->bAffectIndirectLightingWhileHidden = MeshArchetype->bAffectIndirectLightingWhileHidden;
		}
	}

	// For all preview meshes:
	InMeshComponent->SetCastShadow(false);
	InMeshComponent->SetHiddenInGame(false);
	InMeshComponent->SetVisibility(true);

	InMeshComponent->bVisibleInReflectionCaptures = false;
	InMeshComponent->bVisibleInRayTracing = false;
	InMeshComponent->bVisibleInRealTimeSkyCaptures = false;
}

void UDisplayClusterDisplayDeviceBaseComponent::SetupSceneView(const IDisplayClusterViewportPreview& InViewportPreview, uint32 ContextNum, FSceneViewFamily& InOutViewFamily, FSceneView& InOutView) const
{ }

void UDisplayClusterDisplayDeviceBaseComponent::UpdateDisplayDeviceProxyImpl(IDisplayClusterViewportConfiguration& InConfiguration)
{
	// This component does not interact with the rendering thread
}

#if WITH_EDITOR
void UDisplayClusterDisplayDeviceBaseComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
