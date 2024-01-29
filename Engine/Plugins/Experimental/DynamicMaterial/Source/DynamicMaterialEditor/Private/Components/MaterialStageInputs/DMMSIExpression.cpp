// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageExpression.h"

TArray<TStrongObjectPtr<UClass>> UDMMaterialStageInputExpression::InputExpressions = TArray<TStrongObjectPtr<UClass>>();

UDMMaterialStage* UDMMaterialStageInputExpression::CreateStage(TSubclassOf<UDMMaterialStageExpression> InMaterialStageExpressionClass, UDMMaterialLayerObject* InLayer)
{
	check(InMaterialStageExpressionClass);

	GetAvailableInputExpressions();
	check(InputExpressions.Contains(TStrongObjectPtr<UClass>(InMaterialStageExpressionClass.Get())));

	const FDMUpdateGuard Guard;

	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);

	UDMMaterialStageInputExpression* InputExpression = NewObject<UDMMaterialStageInputExpression>(NewStage, NAME_None, RF_Transactional);
	check(InputExpression);
	InputExpression->SetMaterialStageExpressionClass(InMaterialStageExpressionClass);

	NewStage->SetSource(InputExpression);

	return NewStage;
}

void UDMMaterialStageInputExpression::SetMaterialStageExpressionClass(TSubclassOf<UDMMaterialStageExpression> InMaterialStageExpressionClass)
{
	SetMaterialStageThroughputClass(InMaterialStageExpressionClass);
}

UDMMaterialStageExpression* UDMMaterialStageInputExpression::GetMaterialStageExpression() const
{
	return Cast<UDMMaterialStageExpression>(GetMaterialStageThroughput());
}

const TArray<TStrongObjectPtr<UClass>>& UDMMaterialStageInputExpression::GetAvailableInputExpressions()
{
	if (InputExpressions.IsEmpty())
	{
		GenerateExpressionList();
	}

	return InputExpressions;
}

TSubclassOf<UDMMaterialStageExpression> UDMMaterialStageInputExpression::GetMaterialStageExpressionClass() const
{
	return TSubclassOf<UDMMaterialStageExpression>(GetMaterialStageThroughputClass());
}

void UDMMaterialStageInputExpression::GenerateExpressionList()
{
	InputExpressions.Empty();

	const TArray<TStrongObjectPtr<UClass>>& SourceList = UDMMaterialStageExpression::GetAvailableSourceExpressions();

	for (const TStrongObjectPtr<UClass>& SourceClass : SourceList)
	{
		UDMMaterialStageExpression* StageExpressionCDO = CastChecked<UDMMaterialStageExpression>(SourceClass->GetDefaultObject(true));

		if (StageExpressionCDO->IsInputRequired())
		{
			continue;
		}

		InputExpressions.Add(SourceClass);
	}
}
