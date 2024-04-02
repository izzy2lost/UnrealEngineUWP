// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPBaseColor.h"

#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

void UDMMaterialPropertyBaseColor::AddDefaultRGBLayer(EDMMaterialPropertyType InMaterialProperty, UDMMaterialSlot* InSlot)
{
	UDMMaterialStage* DefaultStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	check(DefaultStage);

	UDMMaterialStage* MaskStage = UDMMaterialStageThroughputLayerBlend::CreateStage();
	check(MaskStage);

	InSlot->AddLayerWithMask(InMaterialProperty, DefaultStage, MaskStage);

	UDMMaterialStageInputExpression* BaseInputExpression = UDMMaterialStageInputExpression::ChangeStageInput_Expression(DefaultStage,
		UDMMaterialStageExpressionTextureSample::StaticClass(), UDMMaterialStageBlendNormal::InputB,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 0,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
}

UDMMaterialPropertyBaseColor::UDMMaterialPropertyBaseColor()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::BaseColor),
		EDMValueType::VT_Float3_RGB)
{
}

bool UDMMaterialPropertyBaseColor::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	return (InModelEditorOnlyData.GetShadingModel() != EDMMaterialShadingModel::Unlit);
}

UMaterialExpression* UDMMaterialPropertyBaseColor::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}

void UDMMaterialPropertyBaseColor::OnSlotAdded(UDMMaterialSlot* InSlot)
{
	AddDefaultRGBLayer(EDMMaterialPropertyType::BaseColor, InSlot);
}
