// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PropertyGenerators/DMMaterialStageFunctionPropertyRowGenerator.h"

#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageInputs/DMMSIFunction.h"
#include "DynamicMaterialEditorModule.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "UI/Widgets/Editor/SDMMaterialComponentEditor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageFunctionPropertyRowGenerator"

const TSharedRef<FDMMaterialStageFunctionPropertyRowGenerator>& FDMMaterialStageFunctionPropertyRowGenerator::Get()
{
	static TSharedRef<FDMMaterialStageFunctionPropertyRowGenerator> Generator = MakeShared<FDMMaterialStageFunctionPropertyRowGenerator>();
	return Generator;
}

void FDMMaterialStageFunctionPropertyRowGenerator::ApplyMetaData(const FFunctionExpressionInput& InFunctionInput, 
	const TSharedRef<IPropertyHandle>& InPropertyHandle)
{
	const TArray<FString> MetaNames = {TEXT("UIMin"), TEXT("UIMax"), TEXT("ClampMin"), TEXT("ClampMax")};

	TArray<FString> MetaDatas;
	InFunctionInput.ExpressionInput->Desc.ParseIntoArray(MetaDatas, TEXT(","));

	for (FString& MetaData : MetaDatas)
	{
		const int32 EqualsPosition = MetaData.Find(TEXT("="));

		if (EqualsPosition == INDEX_NONE)
		{
			continue;
		}

		const FString MetaDataName = MetaData.Left(EqualsPosition).TrimStartAndEnd();

		bool bValidName = false;

		for (const FString& Name : MetaNames)
		{
			if (MetaDataName.Equals(Name, ESearchCase::IgnoreCase))
			{
				bValidName = true;
				break;
			}
		}

		if (!bValidName)
		{
			continue;
		}

		const FString MetaDataValue = MetaData.Mid(EqualsPosition + 1).TrimStartAndEnd();
		bool bValidValue = true;

		for (int32 Index = 0; Index < MetaDataValue.Len(); ++Index)
		{
			if (MetaDataValue[Index] != '.'	&& MetaDataValue[Index] != '-' && MetaDataValue[Index] < '0' && MetaDataValue[Index] > '9')
			{
				bValidValue = false;
				break;
			}			
		}

		if (!bValidValue)
		{
			continue;
		}

		InPropertyHandle->SetInstanceMetaData(*MetaDataName, MetaDataValue);
	}
}

void FDMMaterialStageFunctionPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMMaterialComponentEditor>& InComponentEditorWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMMaterialStageFunction* MaterialStageFunction = nullptr;
	TArray<FDMPropertyHandle> AllValuePropertyRows;

	if (UDMMaterialStageInputFunction* StageInputFunction = Cast<UDMMaterialStageInputFunction>(InComponent))
	{
		MaterialStageFunction = StageInputFunction->GetMaterialStageFunction();

		if (MaterialStageFunction)
		{
			InOutProcessedObjects.Add(InComponent);

			if (UMaterialFunctionInterface* MaterialFunction = MaterialStageFunction->GetMaterialFunction())
			{
				TArray<FFunctionExpressionInput> Inputs;
				TArray<FFunctionExpressionOutput> Outputs;
				MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

				TArray<UDMMaterialValue*> InputValues = MaterialStageFunction->GetInputValues();

				// 1 input, the previous stage, does not have a value.
				if (Inputs.Num() != (InputValues.Num() + 1))
				{
					return;
				}

				for (int32 InputIndex = 0; InputIndex < InputValues.Num(); ++InputIndex)
				{
					FFunctionExpressionInput& Input = Inputs[InputIndex + 1];
					UDMMaterialValue* Value = InputValues[InputIndex];

					if (!IsValid(Value))
					{
						continue;
					}

					if (!Input.ExpressionInput)
					{
						continue;
					}

					TArray<FDMPropertyHandle> ValuePropertyRows;

					FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(
						InComponentEditorWidget,
						Value,
						ValuePropertyRows,
						InOutProcessedObjects
					);

					if (ValuePropertyRows.Num() == 1)
					{
						ValuePropertyRows[0].NameOverride = FText::FromName(Input.ExpressionInput->InputName);
					}
					else
					{
						static const FText NameFormat = LOCTEXT("ValueFormat", "{0}[{1}]");

						for (int32 ValuePropertyIndex = 0; ValuePropertyIndex < ValuePropertyRows.Num(); ++ValuePropertyIndex)
						{
							ValuePropertyRows[ValuePropertyIndex].NameOverride = FText::Format(
								NameFormat,
								FText::FromName(Input.ExpressionInput->InputName),
								FText::AsNumber(ValuePropertyIndex + 1)
							);
						}
					}

					const FText Description = FText::FromString(Input.ExpressionInput->Description);

					FText MaterialInputText = FText::FromString(MaterialFunction->GetUserExposedCaption());

					if (MaterialInputText.IsEmpty())
					{
						MaterialInputText = LOCTEXT("Function", "Function");
					}

					const FText MaterialInputFormat = LOCTEXT("MaterialInputFormat", "{0} Inputs");
					MaterialInputText = FText::Format(MaterialInputFormat, MaterialInputText);
					const FName MaterialInputName = *MaterialInputText.ToString();

					for (FDMPropertyHandle& ValuePropertyRow : ValuePropertyRows)
					{
						ValuePropertyRow.NameToolTipOverride = Description;
						ValuePropertyRow.CategoryOverrideName = MaterialInputName;

						if (ValuePropertyRow.PropertyHandle.IsValid())
						{
							// The first input is the previous layer, so it does not have a value.
							ApplyMetaData(Inputs[InputIndex + 1], ValuePropertyRow.PropertyHandle.ToSharedRef());
						}
					}

					AllValuePropertyRows.Append(ValuePropertyRows);
				}
			}
		}
	}

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(
		InComponentEditorWidget, 
		MaterialStageFunction, 
		InOutPropertyRows, 
		InOutProcessedObjects
	);

	InOutPropertyRows.Append(AllValuePropertyRows);
}

#undef LOCTEXT_NAMESPACE
