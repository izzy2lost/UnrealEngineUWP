// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"

#include "DisplayClusterDisplayDeviceBaseComponent.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;

UCLASS(Abstract, ClassGroup = (DisplayCluster))
class DISPLAYCLUSTER_API UDisplayClusterDisplayDeviceBaseComponent
	: public USceneComponent
{
	GENERATED_BODY()
public:
	UDisplayClusterDisplayDeviceBaseComponent();

	/** Retrieve the preview material used. */
	TObjectPtr<UMaterial> GetPreviewMaterial() const { return PreviewMaterial; }

	/** Retrieve the base mesh material. */
	TObjectPtr<UMaterial> GetMeshMaterial() const { return MeshMaterial; }

	/** Perform any operations on the preview material instance, such as setting parameter values. */
	virtual void OnUpdatePreviewMaterialInstance(UMaterialInstanceDynamic* InMaterialInstance) {}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
protected:
	/** The material the preview components will create a material instance from when rendering the nDisplay preview. */
	UPROPERTY(EditAnywhere, Category=Material, NoClear)
	TObjectPtr<UMaterial> PreviewMaterial = nullptr;

	/** The material to assign to the static mesh when preview is disabled. */
	UPROPERTY(EditAnywhere, Category=Material, NoClear)
	TObjectPtr<UMaterial> MeshMaterial = nullptr;
};

UCLASS(ClassGroup = (DisplayCluster), meta = (BlueprintSpawnableComponent, DisplayName = "NDisplay Display Device"))
class DISPLAYCLUSTER_API UDisplayClusterDisplayDeviceComponent
	: public UDisplayClusterDisplayDeviceBaseComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterDisplayDeviceComponent();
	
	virtual void OnUpdatePreviewMaterialInstance(UMaterialInstanceDynamic* InMaterialInstance) override;
	
protected:
	/** Adjust the exposure for the emissive input. */
	UPROPERTY(EditAnywhere, Category=Material)
	float Exposure = 0.f;
};
