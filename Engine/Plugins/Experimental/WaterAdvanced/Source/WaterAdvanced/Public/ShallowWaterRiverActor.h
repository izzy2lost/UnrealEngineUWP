// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

#include "Math/Float16Color.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "NiagaraBakerOutput.h"
#include "NiagaraBakerSettings.h"
#include "NiagaraSystem.h"
#include "UObject/GCObject.h"
#include "ShallowWaterRiverActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class AWaterBody;
class UTextureRenderTarget2D;

UCLASS(BlueprintType, HideCategories = (Physics, Replication, Input, Collision))
class WATERADVANCED_API UShallowWaterRiverComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Niagara River Simulation"))
	TObjectPtr <class UNiagaraSystem> NiagaraRiverSimulation;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Resolution Max Axis"))
	int ResolutionMaxAxis;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Source Width"))
	float SourceSize;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Speed"))
	float SimSpeed = 1.f;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Num Substeps"))
	int NumSteps = 1;

	UPROPERTY(EditAnywhere, Category = "Water", meta = (DisplayName = "Source River Water Body"))
	TObjectPtr<AWaterBody> SourceRiverWaterBody;

	UPROPERTY(EditAnywhere, Category = "Water", meta = (DisplayName = "Sink River Water Body"))
	TObjectPtr<AWaterBody> SinkRiverWaterBody;

	UPROPERTY(EditAnywhere, Category = "Water", meta = (DisplayName = "Additional River Water Bodies"))
	TArray<TObjectPtr<AWaterBody>> AdditonalRiverWaterBodies;

	UPROPERTY(EditAnywhere, Category = "Baking", meta = (DisplayName = "Preview Baked Sim"))
	bool PreviewBakedSim = false;

	UPROPERTY(EditAnywhere, Category = "Shallow Water")
	TObjectPtr<UTexture2D> BakedWaterSurfaceTexture;

	UPROPERTY(EditAnywhere, Category = "Collisions")
	TArray<TObjectPtr<AActor>> BottomContourActors;

	UPROPERTY(EditAnywhere, Category = "Collisions")
	float BottomContourCaptureOffset = 0.f;

	virtual void PostLoad() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnUnregister() override;

#if WITH_EDITOR
	void Rebuild();

	void Bake();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnWaterInfoTextureCreated(const UTextureRenderTarget2D* InWaterInfoTexture);
#endif // WITH_EDITOR

protected:
	// Asset can be set in Project Settings - Plugins - Water ShallowWaterSimulation
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Shallow Water")
	TObjectPtr<UNiagaraComponent> RiverSimSystem;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Shallow Water")
	TObjectPtr<const UTextureRenderTarget2D> WaterInfoTexture;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Shallow Water")
	TObjectPtr<UTextureRenderTarget2D> BakedWaterSurfaceRT;

	bool QueryWaterAtSplinePoint(TObjectPtr<AWaterBody> WaterBody, int SplinePoint, FVector& OutPos, FVector& OutTangent, float& OutWidth, float& OutDepth);

private:
	bool bIsInitialized;	
	bool bTickInitialize;

	UPROPERTY()
	TSet < TObjectPtr<AWaterBody>> AllWaterBodies;

	UPROPERTY()
	FVector2D WorldGridSize;

	UPROPERTY()
	FVector SystemPos;

	UPROPERTY()
	TArray<FVector4> ShallowWaterSimArrayValues;
};

UCLASS(BlueprintType, HideCategories = (Physics, Replication, Input, Collision))
class WATERADVANCED_API AShallowWaterRiver : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	// Asset can be set in Project Settings - Plugins - Water ShallowWaterSimulation
	UPROPERTY(VisibleAnywhere, Category = "Shallow Water")
	TObjectPtr<UShallowWaterRiverComponent> ShallowWaterRiverComponent;	
};