// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterDisplayDeviceBaseComponent.h"
#include "OpenColorIOColorSpace.h"

#include "DisplayClusterDisplayDeviceComponent.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UTexture;
class UTextureRenderTarget2D;

UCLASS(ClassGroup = (DisplayCluster), meta = (BlueprintSpawnableComponent, DisplayName = "NDisplay Display Device"))
class DISPLAYCLUSTER_API UDisplayClusterDisplayDeviceComponent
	: public UDisplayClusterDisplayDeviceBaseComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterDisplayDeviceComponent();

	const FOpenColorIOColorConversionSettings& GetColorConversionSettings() const { return ColorConversionSettings; }
	
	virtual void OnUpdatePreviewMaterialInstance(UMaterialInstanceDynamic* InMaterialInstance) override;
	virtual void RenderPass_GameThread(UTexture* InSourceTexture, UTextureRenderTarget2D* InRenderTarget) override;

protected:
	/** Adjust the exposure for the emissive input. */
	UPROPERTY(EditAnywhere, Category=Material)
	float Exposure = 0.f;
	
	/** Color grading settings. */
	UPROPERTY(EditAnywhere, Category = RenderPass, meta = (DisplayAfter="bEnableRenderPass"))
	FOpenColorIOColorConversionSettings ColorConversionSettings;
};
