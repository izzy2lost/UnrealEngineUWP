// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "CEClonerEffectorSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;
struct FLinearColor;

/** Settings for motion design Cloner and Effector plugin */
UCLASS(Config=Engine, meta=(DisplayName="Cloner & Effector"))
class UCEClonerEffectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static constexpr TCHAR DefaultStaticMeshPath[] = TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'");
	static constexpr TCHAR DefaultMaterialPath[] = TEXT("/Script/Engine.Material'/ClonerEffector/Materials/DefaultClonerMaterial.DefaultClonerMaterial'");

	UCEClonerEffectorSettings();

	/** Inner visualizer color for effectors */
	UPROPERTY(Config, EditAnywhere, Category="Effector")
	FLinearColor VisualizerInnerColor = FLinearColor(255, 0, 0, 0.1);

	/** Outer visualizer color for effectors */
	UPROPERTY(Config, EditAnywhere, Category="Effector")
	FLinearColor VisualizerOuterColor = FLinearColor(0, 0, 255, 0.1);

	/** Spawns a default actor attached to the cloner on spawn */
	UPROPERTY(Config, EditAnywhere, Category="Cloner")
	bool bSpawnDefaultActorAttached = true;

	/** Default static mesh used when spawning default actor attached */
	UPROPERTY(Config, EditAnywhere, Category="Cloner")
	TSoftObjectPtr<UStaticMesh> DefaultStaticMesh;

	/** Default material used when spawning default actor attached */
	UPROPERTY(Config, EditAnywhere, Category="Cloner")
	TSoftObjectPtr<UMaterialInterface> DefaultMaterial;
};
