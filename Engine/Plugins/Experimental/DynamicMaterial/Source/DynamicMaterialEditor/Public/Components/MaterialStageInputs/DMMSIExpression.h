// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMSIExpression.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStageExpression;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputExpression : public UDMMaterialStageInputThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(TSubclassOf<UDMMaterialStageExpression> InMaterialStageExpressionClass, UDMMaterialLayerObject* InLayer = nullptr);

	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableInputExpressions();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSubclassOf<UDMMaterialStageExpression> GetMaterialStageExpressionClass() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialStageExpressionClass(TSubclassOf<UDMMaterialStageExpression> InMaterialStageExpressionClass);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageExpression* GetMaterialStageExpression() const;

protected:
	static TArray<TStrongObjectPtr<UClass>> InputExpressions;

	static void GenerateExpressionList();

	UDMMaterialStageInputExpression() = default;
};
