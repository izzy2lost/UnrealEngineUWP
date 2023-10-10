// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceComponent.h"
#include "Render/Viewport/IDisplayClusterViewportConfiguration.h"

#include "Render/DisplayDevice/Proxy/DisplayClusterDisplayDeviceProxy_OpenColorIO.h"
#include "Render/DisplayDevice/DisplayClusterDisplayDeviceStrings.h"

#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

UDisplayClusterDisplayDeviceComponent::UDisplayClusterDisplayDeviceComponent()
{ }

void UDisplayClusterDisplayDeviceComponent::OnUpdateDisplayDeviceMaterialInstance(IDisplayClusterViewportPreview& InViewportPreview, const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType, UMaterialInstanceDynamic* InMaterialInstance) const
{
	if (!InMaterialInstance || !ShouldUseDisplayDevice(InViewportPreview.GetConfiguration()))
	{
		return;
	}

	switch (InMaterialType)
	{
	case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial:
		InMaterialInstance->SetScalarParameterValue(UE::DisplayClusterDisplayDeviceStrings::material::attr::Exposure, 0.f);
		break;

	case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial:
		InMaterialInstance->SetScalarParameterValue(UE::DisplayClusterDisplayDeviceStrings::material::attr::Exposure, Exposure);
		break;

	default:
		break;
	}

	Super::OnUpdateDisplayDeviceMaterialInstance(InViewportPreview, InMeshType, InMaterialType, InMaterialInstance);
}

void UDisplayClusterDisplayDeviceComponent::UpdateDisplayDeviceProxyImpl(IDisplayClusterViewportConfiguration& InConfiguration)
{
	if (!ShouldUseDisplayDevice(InConfiguration))
	{
		// Use this only for preview rendering
		DisplayDeviceProxy.Reset();

		return;
	}

	if (!bEnableRenderPass || !ColorConversionSettings.IsValid() || !InConfiguration.IsTechvisEnabled())
	{
		// OCIO for preview is disabled
		DisplayDeviceProxy.Reset();

		return;
	}

	// Add a single OCIO render pass at the rendering thread

	// If the settings have changed, create a new proxy object
	if(FDisplayClusterDisplayDeviceProxy_OpenColorIO* Proxy = static_cast<FDisplayClusterDisplayDeviceProxy_OpenColorIO*>(DisplayDeviceProxy.Get()))
	{
		const bool bIsOCIOEquals = Proxy->OCIOPassId == ColorConversionSettings.ToString();
		if (!bIsOCIOEquals)
		{
			DisplayDeviceProxy.Reset();
		}
	}

	if (!DisplayDeviceProxy.IsValid())
	{
		// create a new proxy object for the current settings
		DisplayDeviceProxy = MakeShared<FDisplayClusterDisplayDeviceProxy_OpenColorIO, ESPMode::ThreadSafe>(ColorConversionSettings);
	}
}
