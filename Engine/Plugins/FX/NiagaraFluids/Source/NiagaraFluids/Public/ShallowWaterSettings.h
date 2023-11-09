// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NiagaraSystem.h"
#include "ShallowWaterSettings.generated.h"


UCLASS(config = Engine, defaultconfig, meta=(DisplayName="Water Shallow Water"))
class NIAGARAFLUIDS_API UShallowWaterSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	UShallowWaterSettings();
	
public:
	virtual FName GetCategoryName() const override;
	
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category="Shallow Water Simulation")
	TSoftObjectPtr<UNiagaraSystem> DefaultShallowWaterNiagaraSimulation;
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category="Shallow Water Simulation")
	bool bGPUComputeDebug = false;
};
