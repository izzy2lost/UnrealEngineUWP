// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPOpacity.h"

#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "DynamicMaterialEditorSettings.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

void UDMMaterialPropertyOpacity::AddDefaultOpacityLayer(EDMMaterialPropertyType InMaterialProperty, UDMMaterialSlot* InSlot)
{
	UDMMaterialStage* DefaultStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	check(DefaultStage);

	UDMMaterialStage* MaskStage = UDMMaterialStageThroughputLayerBlend::CreateStage();
	check(MaskStage);

	InSlot->AddLayerWithMask(InMaterialProperty, DefaultStage, MaskStage);

	UDMMaterialStageInputExpression* BaseInputExpression = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
		DefaultStage,
		UDMMaterialStageExpressionTextureSample::StaticClass(),
		UDMMaterialStageBlendNormal::InputB,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		0,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);

	if (UDMMaterialStageExpressionTextureSample* BaseInputTextureSample = Cast<UDMMaterialStageExpressionTextureSample>(BaseInputExpression->GetMaterialStageExpression()))
	{
		BaseInputTextureSample->SetClampTextureEnabled(true);

		if (UDMMaterialStage* BaseTextureInputStage = BaseInputExpression->GetSubStage())
		{
			const TArray<UDMMaterialStageInput*> BaseTextureStageInputs = BaseTextureInputStage->GetInputs();

			for (UDMMaterialStageInput* BaseTextureStageInput : BaseTextureStageInputs)
			{
				if (UDMMaterialStageInputValue* BaseTextureInputValue = Cast<UDMMaterialStageInputValue>(BaseTextureStageInput))
				{
					if (UDMMaterialValueTexture* BaseTextureValue = Cast<UDMMaterialValueTexture>(BaseTextureInputValue->GetValue()))
					{
						BaseTextureValue->SetDefaultValue(UDynamicMaterialEditorSettings::Get()->DefaultOpacitySlotMask.LoadSynchronous());
						BaseTextureValue->ApplyDefaultValue();
						break;
					}
				}
			}
		}
	}

	const TArray<UDMMaterialStageInput*> MaskStageInputs = MaskStage->GetInputs();

	for (UDMMaterialStageInput* MaskStageInput : MaskStageInputs)
	{
		if (UDMMaterialStageInputExpression* MaskInputExpression = Cast<UDMMaterialStageInputExpression>(MaskStageInput))
		{
			if (UDMMaterialStageExpressionTextureSample* MaskInputTextureSample = Cast<UDMMaterialStageExpressionTextureSample>(MaskInputExpression->GetMaterialStageExpression()))
			{
				MaskInputTextureSample->SetClampTextureEnabled(true);

				if (UDMMaterialStage* MaskTextureInputStage = MaskInputExpression->GetSubStage())
				{
					const TArray<UDMMaterialStageInput*> MaskTextureStageInputs = MaskTextureInputStage->GetInputs();

					for (UDMMaterialStageInput* MaskTextureStageInput : MaskTextureStageInputs)
					{
						if (UDMMaterialStageInputValue* MaskTextureInputValue = Cast<UDMMaterialStageInputValue>(MaskTextureStageInput))
						{
							if (UDMMaterialValueTexture* MaskTextureValue = Cast<UDMMaterialValueTexture>(MaskTextureInputValue->GetValue()))
							{
								MaskTextureValue->SetDefaultValue(UDynamicMaterialEditorSettings::Get()->DefaultOpaqueTexture.LoadSynchronous());
								MaskTextureValue->ApplyDefaultValue();
							}
						}
					}
				}
			}
		}
	}
}

UDMMaterialPropertyOpacity::UDMMaterialPropertyOpacity()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Opacity),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyOpacity::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_Translucent:
		case EBlendMode::BLEND_Additive:
		case EBlendMode::BLEND_AlphaComposite:
		case EBlendMode::BLEND_AlphaHoldout:
			return true;

		default:
			return false;
	}
}

UMaterialExpression* UDMMaterialPropertyOpacity::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 1.f);
}

void UDMMaterialPropertyOpacity::OnSlotAdded(UDMMaterialSlot* InSlot)
{
	AddDefaultOpacityLayer(EDMMaterialPropertyType::Opacity, InSlot);
}
