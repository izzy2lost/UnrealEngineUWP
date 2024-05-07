// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
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
	UNiagaraSystem *NiagaraRiverSimulation;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Resolution Max Axis"))
	int ResolutionMaxAxis;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Source Width"))
	float SourceSize;

	UPROPERTY(EditAnywhere, Category = "Water", meta = (DisplayName = "Source River Water Body"))
	TObjectPtr<AWaterBody> SourceRiverWaterBody;

	UPROPERTY(EditAnywhere, Category = "Water", meta = (DisplayName = "Sink River Water Body"))
	TObjectPtr<AWaterBody> SinkRiverWaterBody;

	UPROPERTY(EditAnywhere, Category = "Water", meta = (DisplayName = "Additional River Water Bodies"))
	TArray<TObjectPtr<AWaterBody>> AdditonalRiverWaterBodies;

	UPROPERTY(EditAnywhere, Category = "Reset", meta = (DisplayName = "Reset"))
	bool Reset;

	virtual void PostLoad() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	void Rebuild();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnWaterInfoTextureCreated(const UTextureRenderTarget2D* InWaterInfoTexture);
#endif // WITH_EDITOR

protected:
	// Asset can be set in Project Settings - Plugins - Water ShallowWaterSimulation
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Shallow Water")
	TObjectPtr<UNiagaraComponent> RiverSimSystem;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Shallow Water")
	TObjectPtr<const UTextureRenderTarget2D> WaterInfoTexture;

	bool QueryWaterAtSplinePoint(TObjectPtr<AWaterBody> WaterBody, int SplinePoint, FVector& OutPos, FVector& OutTangent, float& OutWidth, float& OutDepth);

private:
	bool bIsInitialized;	
	bool bTickInitialize;
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