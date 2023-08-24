// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"

#include "DisplayClusterDisplayDeviceBaseComponent.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UTexture;
class UTextureRenderTarget2D;

/**
 * Display Device Components can be added to nDisplay root actors and assigned to viewport nodes to allow additional
 * processing on the preview material.
 */
UCLASS(Abstract, ClassGroup = (DisplayCluster))
class DISPLAYCLUSTER_API UDisplayClusterDisplayDeviceBaseComponent
	: public USceneComponent
{
	GENERATED_BODY()
public:
	UDisplayClusterDisplayDeviceBaseComponent();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	/** Retrieve the preview material used. */
	TObjectPtr<UMaterial> GetPreviewMaterial() const { return PreviewMaterial; }

	/** Retrieve the base mesh material. */
	TObjectPtr<UMaterial> GetMeshMaterial() const { return MeshMaterial; }

	/** If the display device is configured for additional render passes. */
	bool IsRenderPassEnabled() const { return bEnableRenderPass; }
	
	/** Perform any operations on the preview material instance, such as setting parameter values. */
	virtual void OnUpdatePreviewMaterialInstance(UMaterialInstanceDynamic* InMaterialInstance) {}
	
	/**
	 * Start a render pass from the game thread.
	 * @param InSourceTexture The source texture which should be used as input to any render passes.
	 * @param InRenderTarget The destination render target which should be used as output for render passes.
	 */
	virtual void RenderPass_GameThread(UTexture* InSourceTexture, UTextureRenderTarget2D* InRenderTarget) {}
	
protected:
	/** If render passes are enabled. */
	UPROPERTY(EditAnywhere, Category=RenderPass)
	bool bEnableRenderPass = false;
	
	/** The material the preview components will create a material instance from when rendering the nDisplay preview. */
	UPROPERTY(EditAnywhere, Category=Material, NoClear)
	TObjectPtr<UMaterial> PreviewMaterial = nullptr;

	/** The material to assign to the static mesh when preview is disabled. */
	UPROPERTY(EditAnywhere, Category=Material, NoClear)
	TObjectPtr<UMaterial> MeshMaterial = nullptr;
};
