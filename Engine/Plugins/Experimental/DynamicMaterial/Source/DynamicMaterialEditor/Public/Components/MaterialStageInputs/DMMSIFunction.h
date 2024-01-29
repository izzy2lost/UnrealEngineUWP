// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "DMMSIFunction.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStageFunction;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputFunction : public UDMMaterialStageInputThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(UDMMaterialLayerObject* InLayer = nullptr);

	void Init();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageFunction* GetMaterialStageFunction() const;

protected:
	UDMMaterialStageInputFunction() = default;
};
