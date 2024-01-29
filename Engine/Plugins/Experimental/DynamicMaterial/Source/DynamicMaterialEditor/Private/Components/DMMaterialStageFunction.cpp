// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageFunction"

TSoftObjectPtr<UMaterialFunctionInterface> UDMMaterialStageFunction::NoOp = TSoftObjectPtr<UMaterialFunctionInterface>(FSoftObjectPath(TEXT(
	"/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_NoOp.MF_DM_NoOp'"
)));

UDMMaterialStage* UDMMaterialStageFunction::CreateStage(UDMMaterialLayerObject* InLayer)
{
	const FDMUpdateGuard Guard;

	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);
	UDMMaterialStageFunction* SourceFunction = NewObject<UDMMaterialStageFunction>(NewStage, StaticClass(), NAME_None, RF_Transactional);
	check(SourceFunction);

	NewStage->SetSource(SourceFunction);

	return NewStage;
}

void UDMMaterialStageFunction::SetMaterialFunction(UMaterialFunctionInterface* InMaterialFunction)
{
	if (MaterialFunction == InMaterialFunction)
	{
		return;
	}

	MaterialFunction = InMaterialFunction;

	OnMaterialFunctionChanged();
}

void UDMMaterialStageFunction::AddDefaultInput(int32 InInputIndex) const
{
	check(InInputIndex == 0);

	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	UDMMaterialLayerObject* Layer = Stage->GetLayer();
	check(Layer);

	EDMMaterialPropertyType StageProperty = Layer->GetMaterialProperty();
	check(StageProperty != EDMMaterialPropertyType::None);

	const UDMMaterialLayerObject* PreviousLayer = Layer->GetPreviousLayer(StageProperty, EDMMaterialLayerStage::Base);

	if (PreviousLayer)
	{
		Stage->ChangeInput_PreviousStage(InInputIndex, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, StageProperty,
			0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
	}
	else
	{
		Stage->ChangeInput_PreviousStage(InInputIndex, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, EDMMaterialPropertyType::EmissiveColor,
			0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
	}
}

bool UDMMaterialStageFunction::CanChangeInput(int32 InputIndex) const
{
	return false;
}

bool UDMMaterialStageFunction::CanChangeInputType(int32 InputIndex) const
{
	return false;
}

bool UDMMaterialStageFunction::IsInputVisible(int32 InputIndex) const
{
	return false;
}

void UDMMaterialStageFunction::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	UMaterialFunctionInterface* ActualMaterialFunction = MaterialFunction;

	if (!IsValid(ActualMaterialFunction))
	{
		ActualMaterialFunction = NoOp.LoadSynchronous();
	}

	if (!ActualMaterialFunction)
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* FunctionCall = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMaterialFunctionCall>(UE_DM_NodeComment_Default);
	FunctionCall->SetMaterialFunction(ActualMaterialFunction);
	FunctionCall->UpdateFromFunctionResource();

	InBuildState->AddStageSourceExpressions(this, {FunctionCall});
}

void UDMMaterialStageFunction::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	MaterialFunction_PreEdit = MaterialFunction;
}

void UDMMaterialStageFunction::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (MaterialFunction != MaterialFunction_PreEdit)
	{
		OnMaterialFunctionChanged();
	}
}

UDMMaterialStageFunction::UDMMaterialStageFunction()
	: UDMMaterialStageThroughput(LOCTEXT("MaterialFunction", "Material Function"))
{
	MaterialFunction = nullptr;

	bInputRequired = false;
	bAllowNestedInputs = true;

	InputConnectors.Add({InputPreviousStage, LOCTEXT("PreviousStage", "Previous Stage"), EDMValueType::VT_Float3_RGB});

	OutputConnectors.Add({0, LOCTEXT("Output", "Output"), EDMValueType::VT_Float3_RGB});

	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageFunction, MaterialFunction));
}

void UDMMaterialStageFunction::OnMaterialFunctionChanged()
{
	Update(EDMUpdateType::Structure);
}

#undef LOCTEXT_NAMESPACE
