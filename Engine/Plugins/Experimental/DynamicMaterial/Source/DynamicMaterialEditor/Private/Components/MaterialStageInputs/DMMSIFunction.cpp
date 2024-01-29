// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageInputs/DMMSIFunction.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageFunction.h"

UDMMaterialStage* UDMMaterialStageInputFunction::CreateStage(UDMMaterialLayerObject* InLayer)
{
	const FDMUpdateGuard Guard;

	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);

	UDMMaterialStageInputFunction* InputFunction = NewObject<UDMMaterialStageInputFunction>(NewStage, NAME_None, RF_Transactional);
	check(InputFunction);

	InputFunction->Init();

	NewStage->SetSource(InputFunction);

	return NewStage;
}

void UDMMaterialStageInputFunction::Init()
{
	SetMaterialStageThroughputClass(UDMMaterialStageFunction::StaticClass());
}

UDMMaterialStageFunction* UDMMaterialStageInputFunction::GetMaterialStageFunction() const
{
	return Cast<UDMMaterialStageFunction>(GetMaterialStageThroughput());
}
