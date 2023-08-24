// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayDevice/DisplayClusterDisplayDeviceComponent.h"

#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OpenColorIORendering.h"

UDisplayClusterDisplayDeviceComponent::UDisplayClusterDisplayDeviceComponent()
{
}

void UDisplayClusterDisplayDeviceComponent::OnUpdatePreviewMaterialInstance(UMaterialInstanceDynamic* InMaterialInstance)
{
	Super::OnUpdatePreviewMaterialInstance(InMaterialInstance);

	if (InMaterialInstance)
	{
		InMaterialInstance->SetScalarParameterValue(TEXT("Exposure"), Exposure);
	}
}

void UDisplayClusterDisplayDeviceComponent::RenderPass_GameThread(UTexture* InSourceTexture, UTextureRenderTarget2D* InRenderTarget)
{
	Super::RenderPass_GameThread(InSourceTexture, InRenderTarget);

	FOpenColorIORendering::ApplyColorTransform(GetWorld(), GetColorConversionSettings(), InSourceTexture, InRenderTarget);
}
