// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "DMValueDefinition.h"
#include "DynamicMaterialEditorSettings.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Utils/DMPrivate.h"

#define LOCTEXT_NAMESPACE "DMMaterialProperty"

UDMMaterialProperty::UDMMaterialProperty()
	: UDMMaterialProperty(EDMMaterialPropertyType(0), EDMValueType::VT_Float1)
{
}

UDMMaterialProperty::UDMMaterialProperty(EDMMaterialPropertyType InMaterialProperty, EDMValueType InInputConnectorType)
	: MaterialProperty(InMaterialProperty)
	, InputConnectorType(InInputConnectorType)
{
}

FString UDMMaterialProperty::GetComponentPathComponent() const
{
	return StaticEnum<EDMMaterialPropertyType>()->GetNameStringByValue(static_cast<int64>(MaterialProperty));
}

UDMMaterialProperty* UDMMaterialProperty::CreateCustomMaterialPropertyDefaultSubobject(UDynamicMaterialModelEditorOnlyData* InModelEditorOnlyData, EDMMaterialPropertyType InMaterialProperty, const FName& InSubObjName)
{
	check(InModelEditorOnlyData);
	check(UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty));

	UDMMaterialProperty* NewMaterialProperty = InModelEditorOnlyData->CreateDefaultSubobject<UDMMaterialProperty>(InSubObjName);
	NewMaterialProperty->MaterialProperty = InMaterialProperty;
	NewMaterialProperty->InputConnectorType = EDMValueType::VT_None;

	return NewMaterialProperty;
}

UDynamicMaterialModelEditorOnlyData* UDMMaterialProperty::GetMaterialModelEditorOnlyData() const
{
	return Cast<UDynamicMaterialModelEditorOnlyData>(GetOuterSafe());
}

FText UDMMaterialProperty::GetDescription() const
{
	return StaticEnum<EDMMaterialPropertyType>()->GetDisplayNameTextByValue(static_cast<int64>(MaterialProperty));
}

void UDMMaterialProperty::ResetInputConnectionMap()
{
	if (!IsComponentValid())
	{
		return;
	}

	InputConnectionMap.Channels.Empty();

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	UDMMaterialSlot* Slot = ModelEditorOnlyData->GetSlotForMaterialProperty(MaterialProperty);

	if (!Slot || Slot->GetLayers().IsEmpty())
	{
		return;
	}

	const TArray<EDMValueType>& SlotOutputTypes = Slot->GetOutputConnectorTypesForMaterialProperty(MaterialProperty);

	for (int32 SlotOutputIdx = 0; SlotOutputIdx < SlotOutputTypes.Num(); ++SlotOutputIdx)
	{
		if (UDMValueDefinitionLibrary::AreTypesCompatible(SlotOutputTypes[SlotOutputIdx], InputConnectorType))
		{
			InputConnectionMap.Channels.Add({
				FDMMaterialStageConnectorChannel::PREVIOUS_STAGE,
				MaterialProperty,
				SlotOutputIdx,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			});

			break;
		}
	}
}

UMaterialExpression* UDMMaterialProperty::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return nullptr;
}

TEnumAsByte<EMaterialSamplerType> UDMMaterialProperty::GetTextureSamplerType() const
{
	return EMaterialSamplerType::SAMPLERTYPE_LinearColor;
}

void UDMMaterialProperty::OnSlotAdded(UDMMaterialSlot* InSlot)
{
	UDMMaterialStage* DefaultStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	check(DefaultStage);

	UDMMaterialStage* MaskStage = UDMMaterialStageThroughputLayerBlend::CreateStage();
	check(MaskStage);

	InSlot->AddLayerWithMask(MaterialProperty, DefaultStage, MaskStage);

	if (UTexture* BaseTexture = UDynamicMaterialEditorSettings::Get()->GetDefaultTextureForSlot(MaterialProperty))
	{
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
			if (UDMMaterialStage* BaseTextureInputStage = BaseInputExpression->GetSubStage())
			{
				const TArray<UDMMaterialStageInput*> BaseTextureStageInputs = BaseTextureInputStage->GetInputs();

				for (UDMMaterialStageInput* BaseTextureStageInput : BaseTextureStageInputs)
				{
					if (UDMMaterialStageInputValue* BaseTextureInputValue = Cast<UDMMaterialStageInputValue>(BaseTextureStageInput))
					{
						if (UDMMaterialValueTexture* BaseTextureValue = Cast<UDMMaterialValueTexture>(BaseTextureInputValue->GetValue()))
						{
							BaseTextureValue->SetDefaultValue(BaseTexture);
							BaseTextureValue->ApplyDefaultValue();
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			DefaultStage,
			UDMMaterialStageBlendNormal::InputB,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			EDMValueType::VT_Float3_RGB,
			0
		);
	}

	if (UTexture* MaskTexture = UDynamicMaterialEditorSettings::Get()->DefaultMask.LoadSynchronous())
	{
		const TArray<UDMMaterialStageInput*> MaskStageInputs = MaskStage->GetInputs();

		for (UDMMaterialStageInput* MaskStageInput : MaskStageInputs)
		{
			if (UDMMaterialStageInputExpression* MaskInputExpression = Cast<UDMMaterialStageInputExpression>(MaskStageInput))
			{
				if (UDMMaterialStageExpressionTextureSample* MaskInputTextureSample = Cast<UDMMaterialStageExpressionTextureSample>(MaskInputExpression->GetMaterialStageExpression()))
				{
					if (UDMMaterialStage* MaskTextureInputStage = MaskInputExpression->GetSubStage())
					{
						const TArray<UDMMaterialStageInput*> MaskTextureStageInputs = MaskTextureInputStage->GetInputs();

						for (UDMMaterialStageInput* MaskTextureStageInput : MaskTextureStageInputs)
						{
							if (UDMMaterialStageInputValue* MaskTextureInputValue = Cast<UDMMaterialStageInputValue>(MaskTextureStageInput))
							{
								if (UDMMaterialValueTexture* MaskTextureValue = Cast<UDMMaterialValueTexture>(MaskTextureInputValue->GetValue()))
								{
									MaskTextureValue->SetDefaultValue(MaskTexture);
									MaskTextureValue->ApplyDefaultValue();
								}
							}
						}
					}
				}
			}
		}
	}
}

void UDMMaterialProperty::Update(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (HasComponentBeenRemoved())
	{
		return;
	}

	Super::Update(InUpdateType);

	if (InUpdateType == EDMUpdateType::Structure)
	{
		UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = GetMaterialModelEditorOnlyData();
		check(ModelEditorOnlyData);

		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDMMaterialProperty::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModel);

	if (ModelEditorOnlyData && GetOuter() != ModelEditorOnlyData)
	{
		Rename(nullptr, ModelEditorOnlyData, UE::DynamicMaterial::RenameFlags);
	}
}

void UDMMaterialProperty::PreEditChange(FProperty* InPropertyAboutToChange)
{
	Super::PreEditChange(InPropertyAboutToChange);

	static const FName OutputProcessorName = GET_MEMBER_NAME_CHECKED(UDMMaterialProperty, OutputProcessor);

	if (InPropertyAboutToChange->GetFName() == OutputProcessorName)
	{
		OutputProcessor_PreUpdate = OutputProcessor;
	}
}

void UDMMaterialProperty::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	static const FName OutputProcessorName = GET_MEMBER_NAME_CHECKED(UDMMaterialProperty, OutputProcessor);

	if (InPropertyChangedEvent.Property && InPropertyChangedEvent.Property->GetFName() == OutputProcessorName)
	{
		OnOutputProcessorUpdated();
	}
}

void UDMMaterialProperty::LoadDeprecatedModelData(UDMMaterialProperty* InOldProperty)
{
	InputConnectionMap = InOldProperty->InputConnectionMap;
	OutputProcessor = InOldProperty->OutputProcessor;
}

void UDMMaterialProperty::SetOutputProcessor(UMaterialFunctionInterface* InFunction)
{
	if (OutputProcessor == InFunction)
	{
		return;
	}

	OutputProcessor_PreUpdate = OutputProcessor;
	OutputProcessor = InFunction;

	OnOutputProcessorUpdated();
}

void UDMMaterialProperty::OnOutputProcessorUpdated()
{
	if (!OutputProcessor)
	{
		if (OutputProcessor_PreUpdate)
		{
			Update(EDMUpdateType::Structure);
		}

		OutputProcessor = nullptr;
		OutputProcessor_PreUpdate = nullptr;
		return;
	}

	using namespace UE::DynamicMaterialEditor;

	bool bValid = true;

	do
	{
		TArray<FFunctionExpressionInput> Inputs;
		TArray<FFunctionExpressionOutput> Outputs;

		OutputProcessor->GetInputsAndOutputs(Inputs, Outputs);

		if (Inputs.IsEmpty() || Outputs.IsEmpty())
		{
			bValid = false;
			break;
		}
	}
	while (false);

	if (!bValid)
	{
		if (IsValid(OutputProcessor_PreUpdate))
		{
			// No update has occurred
			OutputProcessor = OutputProcessor_PreUpdate;
			OutputProcessor_PreUpdate = nullptr;
			return;
		}
		else
		{
			// Possible update has occurred
			OutputProcessor = nullptr;
			OutputProcessor_PreUpdate = nullptr;
		}
	}

	Update(EDMUpdateType::Structure);
}

UMaterialExpression* UDMMaterialProperty::CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState,
	float InDefaultValue)
{
	UMaterialExpressionConstant* Constant = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant>(UE_DM_NodeComment_Default);
	Constant->R = InDefaultValue;

	InBuildState->AddOtherExpressions({Constant});

	return Constant;
}

UMaterialExpression* UDMMaterialProperty::CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState,
	const FVector2d& InDefaultValue)
{
	UMaterialExpressionConstant2Vector* Constant = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant2Vector>(UE_DM_NodeComment_Default);
	Constant->R = InDefaultValue.X;
	Constant->G = InDefaultValue.Y;

	InBuildState->AddOtherExpressions({Constant});

	return Constant;
}

UMaterialExpression* UDMMaterialProperty::CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState,
	const FVector3d& InDefaultValue)
{
	UMaterialExpressionConstant3Vector* Constant = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant3Vector>(UE_DM_NodeComment_Default);
	Constant->Constant.R = InDefaultValue.X;
	Constant->Constant.G = InDefaultValue.Y;
	Constant->Constant.B = InDefaultValue.Z;
	Constant->Constant.A = 0.f;

	InBuildState->AddOtherExpressions({Constant});

	return Constant;
}

UMaterialExpression* UDMMaterialProperty::CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState,
	const FVector4d& InDefaultValue)
{
	UMaterialExpressionConstant4Vector* Constant = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant4Vector>(UE_DM_NodeComment_Default);
	Constant->Constant.R = InDefaultValue.X;
	Constant->Constant.G = InDefaultValue.Y;
	Constant->Constant.B = InDefaultValue.Z;
	Constant->Constant.A = InDefaultValue.W;

	InBuildState->AddOtherExpressions({Constant});

	return Constant;
}

#undef LOCTEXT_NAMESPACE
