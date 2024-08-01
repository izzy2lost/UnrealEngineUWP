// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PropertyGenerators/DMMaterialModelPropertyRowGenerator.h"

#include "Components/DMMaterialProperty.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "UI/Utils/DMWidgetStatics.h"
#include "UI/Widgets/Editor/SDMMaterialGlobalSettingsEditor.h"

#define LOCTEXT_NAMESPACE "DMMaterialModelPropertyRowGenerator"

void FDMMaterialModelPropertyRowGenerator::AddMaterialModelProperties(const TSharedRef<SDMMaterialGlobalSettingsEditor>& InGlobalSettingEditorWidget, 
	UDynamicMaterialModelBase* InMaterialModelBase, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	UDynamicMaterialModel* MaterialModel = InMaterialModelBase->ResolveMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	AddGlobalValue(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, 
		MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOffsetValueName), 
		LOCTEXT("GlobalOffset", "Global Offset"));

	AddGlobalValue(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows,
		MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalTilingValueName),
		LOCTEXT("GlobalTiling", "Global Tiling"));

	AddGlobalValue(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows,
		MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalRotationValueName), 
		LOCTEXT("GlobalRotation", "Global Rotation"));

	if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModelBase))
	{
		if (EditorOnlyData->GetBlendMode() != BLEND_Opaque)
		{
			AddGlobalValue(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows,  
				MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOpacityValueName), 
				LOCTEXT("GlobalOpacity", "Global Opacity"));
		}

		EditorOnlyData->ForEachMaterialPropertyType(
			[&InGlobalSettingEditorWidget, InMaterialModelBase, &InOutPropertyRows, EditorOnlyData]
			(EDMMaterialPropertyType InProperty)
			{
				return ForEachProperty(InProperty, InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData);
			},
			/* Start from */ EDMMaterialPropertyType::Roughness
		);
	}

	if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModelBase))
	{
		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData, 
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, ChannelListPreset));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, Domain));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, BlendMode));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, ShadingModel));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bHasPixelAnimation));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bTwoSided));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bResponsiveAAEnabled));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bOutputTranslucentVelocityEnabled));

		AddVariable(InGlobalSettingEditorWidget, InMaterialModelBase, InOutPropertyRows, EditorOnlyData,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bNaniteTessellationEnabled));
	}
}

EDMIterationResult FDMMaterialModelPropertyRowGenerator::ForEachProperty(EDMMaterialPropertyType InProperty, 
	const TSharedRef<SDMMaterialGlobalSettingsEditor>& InGlobalSettingEditorWidget, UDynamicMaterialModelBase* InMaterialModelBase, 
	TArray<FDMPropertyHandle>& InOutPropertyRows, UDynamicMaterialModelEditorOnlyData* InEditorOnlyData)
{
	if (!InEditorOnlyData->GetSlotForMaterialProperty(InProperty))
	{
		if (UDMMaterialProperty* MaterialProperty = InEditorOnlyData->GetMaterialProperty(InProperty))
		{
			if (MaterialProperty->IsValidForModel(*InEditorOnlyData))
			{
				if (UDMMaterialValueFloat1* AlphaValue = Cast<UDMMaterialValueFloat1>(MaterialProperty->GetComponent(UDynamicMaterialModelEditorOnlyData::AlphaValueName)))
				{
					UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

					AddGlobalValue(
						InGlobalSettingEditorWidget,
						InMaterialModelBase,
						InOutPropertyRows,
						AlphaValue,
						FText::Format(
							LOCTEXT("PropertyFormat", "Global {0}"),
							MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(InProperty))
						)
					);
				}
			}
		}
	}

	return EDMIterationResult::Continue;
}

void FDMMaterialModelPropertyRowGenerator::AddGlobalValue(const TSharedRef<SDMMaterialGlobalSettingsEditor>& InGlobalSettingEditorWidget,
	UDynamicMaterialModelBase* InMaterialModelBase, TArray<FDMPropertyHandle>& InOutPropertyRows, UDMMaterialComponent* InComponent, 
	const FText& InNameOverride)
{
	if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(InMaterialModelBase))
	{
		InComponent = MaterialModelDynamic->GetComponentDynamic(InComponent->GetFName());

		if (!InComponent)
		{
			return;
		}
	}

	FDMPropertyHandle& ComponentHandle = InOutPropertyRows.Add_GetRef(FDMWidgetStatics::Get().GetPropertyHandle(&*InGlobalSettingEditorWidget,
	                                                                                                            InComponent, UDMMaterialValue::ValueName));

	ComponentHandle.CategoryOverrideName = TEXT("Material Settings");
	ComponentHandle.NameOverride = InNameOverride;

	if (UDMMaterialValue* MaterialValue = Cast<UDMMaterialValue>(InComponent))
	{
		ComponentHandle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateUObject(MaterialValue, &UDMMaterialValue::CanResetToDefault),
			FResetToDefaultHandler::CreateUObject(MaterialValue, &UDMMaterialValue::ResetToDefault),
			/* Propagate to children */ false
		);
	}
	else if (UDMMaterialValueDynamic* MaterialValueDynamic = Cast<UDMMaterialValueDynamic>(InComponent))
	{
		ComponentHandle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateUObject(MaterialValueDynamic, &UDMMaterialValueDynamic::CanResetToDefault),
			FResetToDefaultHandler::CreateUObject(MaterialValueDynamic, &UDMMaterialValueDynamic::ResetToDefault),
			/* Propagate to children */ false
		);
	}
}

void FDMMaterialModelPropertyRowGenerator::AddVariable(const TSharedRef<SDMMaterialGlobalSettingsEditor>& InGlobalSettingEditorWidget, 
	UDynamicMaterialModelBase* InMaterialModelBase, TArray<FDMPropertyHandle>& InOutPropertyRows, UObject* InObject, FName InPropertyName)
{
	FDMPropertyHandle& ValueHandle = InOutPropertyRows.Add_GetRef(FDMWidgetStatics::Get().GetPropertyHandle(
		&*InGlobalSettingEditorWidget, 
		InObject, InPropertyName
	));

	ValueHandle.CategoryOverrideName = TEXT("Material Type");
	ValueHandle.bEnabled = !IsDynamic(InMaterialModelBase);
}

bool FDMMaterialModelPropertyRowGenerator::IsDynamic(UDynamicMaterialModelBase* InMaterialModelBase)
{
	return !!Cast<UDynamicMaterialModelDynamic>(InMaterialModelBase);
}

#undef LOCTEXT_NAMESPACE
