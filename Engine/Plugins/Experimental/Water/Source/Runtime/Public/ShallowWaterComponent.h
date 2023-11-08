// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ShallowWaterComponent.generated.h"

class AWaterBody;
class UNiagaraComponent;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WATER_API UShallowWaterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShallowWaterComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;
	
protected:
	// Asset can be set in Project Settings - Plugins - Water ShallowWaterSimulation
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Shallow Water")
	TObjectPtr<UNiagaraComponent> ShallowWaterNiagaraSimulation;

	UFUNCTION()
	void OnOwnerBeginOverlap(AActor* Owner, AActor* OtherActor);
	void UpdateNiagaraVariables();
	
	AWaterBody* CurrentWaterBody = nullptr; 
};
