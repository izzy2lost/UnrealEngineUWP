// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMSIGradient.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStageGradient;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputGradient : public UDMMaterialStageInputThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(TSubclassOf<UDMMaterialStageGradient> InMaterialStageGradientClass, UDMMaterialLayerObject* InLayer = nullptr);

	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableGradients();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSubclassOf<UDMMaterialStageGradient> GetMaterialStageGradientClass() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialStageGradientClass(TSubclassOf<UDMMaterialStageGradient> InMaterialStageGradientClass);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageGradient* GetMaterialStageGradient() const;

protected:
	static TArray<TStrongObjectPtr<UClass>> Gradients;

	static void GenerateGradientList();

	UDMMaterialStageInputGradient() = default;
};
