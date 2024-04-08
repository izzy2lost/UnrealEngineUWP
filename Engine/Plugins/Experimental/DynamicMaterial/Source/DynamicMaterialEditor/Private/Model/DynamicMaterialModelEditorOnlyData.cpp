// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialValue.h"
#include "Components/DMTextureUV.h"
#include "Components/MaterialProperties/DMMPAmbientOcclusion.h"
#include "Components/MaterialProperties/DMMPAnisotropy.h"
#include "Components/MaterialProperties/DMMPBaseColor.h"
#include "Components/MaterialProperties/DMMPEmissiveColor.h"
#include "Components/MaterialProperties/DMMPMetallic.h"
#include "Components/MaterialProperties/DMMPNormal.h"
#include "Components/MaterialProperties/DMMPOpacity.h"
#include "Components/MaterialProperties/DMMPOpacityMask.h"
#include "Components/MaterialProperties/DMMPPixelDepthOffset.h"
#include "Components/MaterialProperties/DMMPRefraction.h"
#include "Components/MaterialProperties/DMMPRoughness.h"
#include "Components/MaterialProperties/DMMPSpecular.h"
#include "Components/MaterialProperties/DMMPTangent.h"
#include "Components/MaterialProperties/DMMPWorldPositionOffset.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "CoreGlobals.h"
#include "DMComponentPath.h"
#include "DMDefs.h"
#include "DMMaterialFunctionLibrary.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialFunction.h"
#include "Misc/Guid.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModel.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerModel"

const FString UDynamicMaterialModelEditorOnlyData::SlotsPathToken               = FString(TEXT("Slots"));
const FString UDynamicMaterialModelEditorOnlyData::RGBSlotPathToken             = FString(TEXT("RGBSlot"));
const FString UDynamicMaterialModelEditorOnlyData::OpacitySlotPathToken         = FString(TEXT("OpacitySlot"));
const FString UDynamicMaterialModelEditorOnlyData::RoughnessPathToken           = FString(TEXT("RoughnessSlot"));
const FString UDynamicMaterialModelEditorOnlyData::SpecularPathToken            = FString(TEXT("SpecularSlot"));
const FString UDynamicMaterialModelEditorOnlyData::MetallicPathToken            = FString(TEXT("MetallicSlot"));
const FString UDynamicMaterialModelEditorOnlyData::NormalPathToken              = FString(TEXT("NormalSlot"));
const FString UDynamicMaterialModelEditorOnlyData::PixelDepthOffsetPathToken    = FString(TEXT("PixelDepthOffsetSlot"));
const FString UDynamicMaterialModelEditorOnlyData::WorldPositionOffsetPathToken = FString(TEXT("WorldPositionOffsetSlot"));
const FString UDynamicMaterialModelEditorOnlyData::AmbientOcclusionPathToken    = FString(TEXT("AmbientOcclusionSlot"));
const FString UDynamicMaterialModelEditorOnlyData::AnisotropyPathToken          = FString(TEXT("AnisotropySlot"));
const FString UDynamicMaterialModelEditorOnlyData::RefractionPathToken          = FString(TEXT("RefractionSlot"));
const FString UDynamicMaterialModelEditorOnlyData::TangentPathToken             = FString(TEXT("TangentSlot"));
const FString UDynamicMaterialModelEditorOnlyData::Custom1PathToken             = FString(TEXT("Custom1Slot"));
const FString UDynamicMaterialModelEditorOnlyData::Custom2PathToken             = FString(TEXT("Custom2Slot"));
const FString UDynamicMaterialModelEditorOnlyData::Custom3PathToken             = FString(TEXT("Custom3Slot"));
const FString UDynamicMaterialModelEditorOnlyData::Custom4PathToken             = FString(TEXT("Custom4Slot"));
const FString UDynamicMaterialModelEditorOnlyData::PropertiesPathToken          = FString(TEXT("Properties"));

namespace UE::DynamicMaterialEditor::Private
{
	const TMap<FString, EDMMaterialPropertyType> TokenToPropertyMap = {
		{UDynamicMaterialModelEditorOnlyData::RGBSlotPathToken,             EDMMaterialPropertyType::BaseColor},
		{UDynamicMaterialModelEditorOnlyData::OpacitySlotPathToken,         EDMMaterialPropertyType::Opacity},
		{UDynamicMaterialModelEditorOnlyData::RoughnessPathToken,           EDMMaterialPropertyType::Roughness},
		{UDynamicMaterialModelEditorOnlyData::SpecularPathToken,            EDMMaterialPropertyType::Specular},
		{UDynamicMaterialModelEditorOnlyData::MetallicPathToken,            EDMMaterialPropertyType::Metallic},
		{UDynamicMaterialModelEditorOnlyData::NormalPathToken,              EDMMaterialPropertyType::Normal},
		{UDynamicMaterialModelEditorOnlyData::PixelDepthOffsetPathToken,    EDMMaterialPropertyType::PixelDepthOffset},
		{UDynamicMaterialModelEditorOnlyData::WorldPositionOffsetPathToken, EDMMaterialPropertyType::WorldPositionOffset},
		{UDynamicMaterialModelEditorOnlyData::AmbientOcclusionPathToken,    EDMMaterialPropertyType::AmbientOcclusion},
		{UDynamicMaterialModelEditorOnlyData::AnisotropyPathToken,          EDMMaterialPropertyType::Anisotropy},
		{UDynamicMaterialModelEditorOnlyData::RefractionPathToken,          EDMMaterialPropertyType::Refraction},
		{UDynamicMaterialModelEditorOnlyData::TangentPathToken,             EDMMaterialPropertyType::Tangent},
		{UDynamicMaterialModelEditorOnlyData::Custom1PathToken,             EDMMaterialPropertyType::Custom1},
		{UDynamicMaterialModelEditorOnlyData::Custom2PathToken,             EDMMaterialPropertyType::Custom2},
		{UDynamicMaterialModelEditorOnlyData::Custom3PathToken,             EDMMaterialPropertyType::Custom3},
		{UDynamicMaterialModelEditorOnlyData::Custom4PathToken,             EDMMaterialPropertyType::Custom4}
	};
}

const TArray<EMaterialDomain> UDynamicMaterialModelEditorOnlyData::SupportedDomains =
{
	EMaterialDomain::MD_Surface,
	EMaterialDomain::MD_PostProcess,
	EMaterialDomain::MD_DeferredDecal
};

const TArray<EBlendMode> UDynamicMaterialModelEditorOnlyData::SupportedBlendModes =
{
	EBlendMode::BLEND_Opaque,
	EBlendMode::BLEND_Masked,
	EBlendMode::BLEND_Translucent,
	EBlendMode::BLEND_Additive,
	EBlendMode::BLEND_Modulate
};

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(UDynamicMaterialModel* InModel)
{
	if (InModel)
	{
		return Get(InModel->GetEditorOnlyData());
	}

	return nullptr;
}

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(TWeakObjectPtr<UDynamicMaterialModel> InModelWeak)
{
	return Get(InModelWeak.Get());
}

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(const TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>& InInterface)
{
	return Cast<UDynamicMaterialModelEditorOnlyData>(InInterface.GetObject());
}

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(IDynamicMaterialModelEditorOnlyDataInterface* InInterface)
{
	return Cast<UDynamicMaterialModelEditorOnlyData>(InInterface);
}

UDynamicMaterialModelEditorOnlyData::UDynamicMaterialModelEditorOnlyData()
{
	State = EDMState::Idle;

	BlendMode = EBlendMode::BLEND_Translucent;
	ShadingModel = EDMMaterialShadingModel::Unlit;
	bPixelAnimationFlag = false;
	bTwoSidedFlag = true;
	ChannelListPreset = NAME_None;

	Properties.Emplace(EDMMaterialPropertyType::BaseColor,           CreateDefaultSubobject<UDMMaterialPropertyBaseColor>(          "MaterialProperty_BaseColor"));
	Properties.Emplace(EDMMaterialPropertyType::EmissiveColor,       CreateDefaultSubobject<UDMMaterialPropertyEmissiveColor>(      "MaterialProperty_EmissiveColor"));
	Properties.Emplace(EDMMaterialPropertyType::Opacity,             CreateDefaultSubobject<UDMMaterialPropertyOpacity>(            "MaterialProperty_Opacity"));
	Properties.Emplace(EDMMaterialPropertyType::OpacityMask,         CreateDefaultSubobject<UDMMaterialPropertyOpacityMask>(        "MaterialProperty_OpacityMask"));
	Properties.Emplace(EDMMaterialPropertyType::Metallic,            CreateDefaultSubobject<UDMMaterialPropertyMetallic>(           "MaterialProperty_Metallic"));
	Properties.Emplace(EDMMaterialPropertyType::Specular,            CreateDefaultSubobject<UDMMaterialPropertySpecular>(           "MaterialProperty_Specular"));
	Properties.Emplace(EDMMaterialPropertyType::Roughness,           CreateDefaultSubobject<UDMMaterialPropertyRoughness>(          "MaterialProperty_Roughness"));
	Properties.Emplace(EDMMaterialPropertyType::Anisotropy,          CreateDefaultSubobject<UDMMaterialPropertyAnisotropy>(         "MaterialProperty_Anisotropy"));
	Properties.Emplace(EDMMaterialPropertyType::Normal,              CreateDefaultSubobject<UDMMaterialPropertyNormal>(             "MaterialProperty_Normal"));
	Properties.Emplace(EDMMaterialPropertyType::Tangent,             CreateDefaultSubobject<UDMMaterialPropertyTangent>(            "MaterialProperty_Tangent"));
	Properties.Emplace(EDMMaterialPropertyType::WorldPositionOffset, CreateDefaultSubobject<UDMMaterialPropertyWorldPositionOffset>("MaterialProperty_WorldPositionOffset"));
	Properties.Emplace(EDMMaterialPropertyType::AmbientOcclusion,    CreateDefaultSubobject<UDMMaterialPropertyAmbientOcclusion>(   "MaterialProperty_AmbientOcclusion"));
	Properties.Emplace(EDMMaterialPropertyType::Refraction,          CreateDefaultSubobject<UDMMaterialPropertyRefraction>(         "MaterialProperty_Refraction"));
	Properties.Emplace(EDMMaterialPropertyType::PixelDepthOffset,    CreateDefaultSubobject<UDMMaterialPropertyPixelDepthOffset>(   "MaterialProperty_PixelDepthOffset"));

	Properties.Emplace(EDMMaterialPropertyType::Custom1, UDMMaterialProperty::CreateCustomMaterialPropertyDefaultSubobject(this, EDMMaterialPropertyType::Custom1, "MaterialProperty_Custom1"));
	Properties.Emplace(EDMMaterialPropertyType::Custom2, UDMMaterialProperty::CreateCustomMaterialPropertyDefaultSubobject(this, EDMMaterialPropertyType::Custom2, "MaterialProperty_Custom2"));
	Properties.Emplace(EDMMaterialPropertyType::Custom3, UDMMaterialProperty::CreateCustomMaterialPropertyDefaultSubobject(this, EDMMaterialPropertyType::Custom3, "MaterialProperty_Custom3"));
	Properties.Emplace(EDMMaterialPropertyType::Custom4, UDMMaterialProperty::CreateCustomMaterialPropertyDefaultSubobject(this, EDMMaterialPropertyType::Custom4, "MaterialProperty_Custom4"));

	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		Pair.Value->SetComponentState(EDMComponentLifetimeState::Added);
	}
}

void UDynamicMaterialModelEditorOnlyData::Initialize()
{
	if (!Slots.IsEmpty())
	{
		return;
	}

	// Will choose appropriate type between BaseColor and EmissiveColor
	AddSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor);
}

UMaterial* UDynamicMaterialModelEditorOnlyData::GetGeneratedMaterial() const
{
	return IsValid(MaterialModel) ? MaterialModel->DynamicMaterial : nullptr;
}

void UDynamicMaterialModelEditorOnlyData::ResetData()
{
	if (GUndo)
	{
		Modify();
	}

	BlendMode = EBlendMode::BLEND_Translucent;
	ShadingModel = EDMMaterialShadingModel::Unlit;
	Slots.Empty();
	PropertySlotMap.Empty();
	Expressions.Empty();
}

void UDynamicMaterialModelEditorOnlyData::CreateMaterial()
{
	if (!IsValid(MaterialModel))
	{
		return;
	}

	if (FDynamicMaterialModule::IsMaterialExportEnabled() == false)
	{
		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		check(MaterialFactory);

		MaterialModel->DynamicMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			MaterialModel,
			NAME_None,
			RF_DuplicateTransient | RF_TextExportTransient,
			nullptr,
			GWarn
		));
	}
	else
	{
		FString MaterialBaseName = GetName() + "-" + FGuid::NewGuid().ToString();
		const FString FullName = "/Game/DynamicMaterials/" + MaterialBaseName;
		UPackage* Package = CreatePackage(*FullName);

		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		check(MaterialFactory);

		MaterialModel->DynamicMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Package,
			*MaterialBaseName,
			RF_DuplicateTransient | RF_TextExportTransient | RF_Standalone | RF_Public,
			nullptr,
			GWarn
		));

		FAssetRegistryModule::AssetCreated(MaterialModel->DynamicMaterial);
		Package->FullyLoad();
	}

	MaterialModel->DynamicMaterial->bOutputTranslucentVelocity = true;
	MaterialModel->DynamicMaterial->bEnableResponsiveAA = true;

	// Not setting this to true can cause the level associated with this material to dirty itself when it
	// is used with Niagara. It doesn't negatively affect the material in any meaningful way.
	MaterialModel->DynamicMaterial->bUsedWithNiagaraMeshParticles = true;
}

void UDynamicMaterialModelEditorOnlyData::BuildMaterial(bool bInDirtyAssets)
{
	if (State != EDMState::Idle)
	{
		checkNoEntry();
		return;
	}

	if (!IsValid(MaterialModel))
	{
		return;
	}

	UE_LOG(LogDynamicMaterialEditor, Display, TEXT("Building Material Designer Material (%s)..."), *MaterialModel->GetFullName());

	if (!IsValid(MaterialModel->DynamicMaterial))
	{
		CreateMaterial();
	}

	State = EDMState::Building;
	Expressions.Empty();
	MaterialModel->DynamicMaterial->MaterialDomain = Domain;
	MaterialModel->DynamicMaterial->BlendMode = BlendMode;
	MaterialModel->DynamicMaterial->bHasPixelAnimation = bPixelAnimationFlag;
	MaterialModel->DynamicMaterial->TwoSided = bTwoSidedFlag;

	switch (ShadingModel)
	{
		case EDMMaterialShadingModel::DefaultLit:
			MaterialModel->DynamicMaterial->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
			break;

		case EDMMaterialShadingModel::Unlit:
			MaterialModel->DynamicMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
			break;

		default:
			checkNoEntry();
			break;
	}

	TSharedRef<FDMMaterialBuildState> BuildState = CreateBuildState(MaterialModel->DynamicMaterial, bInDirtyAssets);

	const bool bIsPostProcessMaterial = Domain == EMaterialDomain::MD_PostProcess;

	/**
	 * Process slots to build base material inputs.
	 */
	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(Pair.Key))
		{
			continue;
		}

		if (bIsPostProcessMaterial && Pair.Key != EDMMaterialPropertyType::EmissiveColor)
		{
			continue;
		}

		// For now we don't have channel remapping!
		FExpressionInput* MaterialPropertyPtr = BuildState->GetMaterialProperty(Pair.Key);

		if (!MaterialPropertyPtr)
		{
			continue;
		}

		MaterialPropertyPtr->Expression = nullptr;
		MaterialPropertyPtr->OutputIndex = 0;

		UMaterialExpression* LastPropertyExpression = nullptr;
		UDMMaterialSlot* Slot = GetSlotForMaterialProperty(Pair.Key);

		if (!Slot || Slot->GetLayers().IsEmpty())
		{
			continue;
		}

		Slot->GenerateExpressions(BuildState);

		if (BuildState->GetSlotExpressions(Slot).IsEmpty())
		{
			continue;
		}

		LastPropertyExpression = BuildState->GetLastSlotPropertyExpression(Slot, Pair.Key);

		if (!LastPropertyExpression)
		{
			continue;
		}

		MaterialPropertyPtr->Expression = LastPropertyExpression;

		if (Pair.Value->GetInputConnectionMap().Channels.IsEmpty() == false)
		{
			MaterialPropertyPtr->OutputIndex = Pair.Value->GetInputConnectionMap().Channels[0].OutputIndex;
		}
		else
		{
			MaterialPropertyPtr->OutputIndex = 0;
		}
	}

	if (!bIsPostProcessMaterial)
	{
		/**
		 * Generate opacity input based on base/emissive if it doesn't already have an input.
		 */
		EDMMaterialPropertyType GenerateOpacityInput = EDMMaterialPropertyType::None;

		if (BlendMode == EBlendMode::BLEND_Translucent)
		{
			if (GetSlotForMaterialProperty(EDMMaterialPropertyType::Opacity) == nullptr)
			{
				GenerateOpacityInput = EDMMaterialPropertyType::Opacity;
			}
		}
		else if (BlendMode == EBlendMode::BLEND_Masked)
		{
			if (GetSlotForMaterialProperty(EDMMaterialPropertyType::OpacityMask) == nullptr)
			{
				GenerateOpacityInput = EDMMaterialPropertyType::OpacityMask;
			}
		}

		if (GenerateOpacityInput != EDMMaterialPropertyType::None)
		{
			UDMMaterialSlot* OpacitySlot = nullptr;
			EDMMaterialPropertyType OpacityProperty = EDMMaterialPropertyType::None;

			if (UDMMaterialSlot* BaseColorSlot = GetSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor))
			{
				OpacitySlot = BaseColorSlot;
				OpacityProperty = EDMMaterialPropertyType::BaseColor;
			}
			else if (UDMMaterialSlot* EmissiveSlot = GetSlotForMaterialProperty(EDMMaterialPropertyType::EmissiveColor))
			{
				OpacitySlot = EmissiveSlot;
				OpacityProperty = EDMMaterialPropertyType::EmissiveColor;
			}

			if (OpacitySlot)
			{
				UMaterialExpression* OpacityOutputNode;
				int32 OutputIndex;
				int32 OutputChannel;
				GenerateOpacityExpressions(BuildState, OpacitySlot, OpacityProperty, OpacityOutputNode, OutputIndex, OutputChannel);

				if (OpacityOutputNode)
				{
					if (FExpressionInput* OpacityPropertyPtr = BuildState->GetMaterialProperty(GenerateOpacityInput))
					{
						OpacityPropertyPtr->Expression = OpacityOutputNode;
						OpacityPropertyPtr->OutputIndex = 0;
						OpacityPropertyPtr->Mask = 0;

						if (OutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
						{
							OpacityPropertyPtr->Mask = 1;
							OpacityPropertyPtr->MaskR = !!(OutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL);
							OpacityPropertyPtr->MaskG = !!(OutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL);
							OpacityPropertyPtr->MaskB = !!(OutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL);
							OpacityPropertyPtr->MaskA = !!(OutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL);
						}
					}
				}
			}
		}

		/**
		 * Apply global opacity slider
		 */
		FExpressionInput* OpacityPropertyPtr = nullptr;

		if (BlendMode == BLEND_Translucent)
		{
			OpacityPropertyPtr = BuildState->GetMaterialProperty(EDMMaterialPropertyType::Opacity);
		}
		else if (BlendMode == BLEND_Masked)
		{
			OpacityPropertyPtr = BuildState->GetMaterialProperty(EDMMaterialPropertyType::OpacityMask);
		}

		if (OpacityPropertyPtr != nullptr && IsValid(MaterialModel) && IsValid(MaterialModel->GlobalOpacityValue))
		{
			MaterialModel->GlobalOpacityValue->GenerateExpression(BuildState);

			UMaterialExpression* GlobalOpacityExpression = BuildState->GetLastValueExpression(MaterialModel->GlobalOpacityValue);

			if (OpacityPropertyPtr->Expression)
			{
				UMaterialExpressionMultiply* OpacityMultiply = BuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMultiply>(UE_DM_NodeComment_Default);
				OpacityMultiply->A.Expression = OpacityPropertyPtr->Expression;
				OpacityMultiply->A.Mask = OpacityPropertyPtr->Mask;
				OpacityMultiply->A.MaskR = OpacityPropertyPtr->MaskR;
				OpacityMultiply->A.MaskG = OpacityPropertyPtr->MaskG;
				OpacityMultiply->A.MaskB = OpacityPropertyPtr->MaskB;
				OpacityMultiply->A.MaskA = OpacityPropertyPtr->MaskA;
				OpacityMultiply->A.OutputIndex = OpacityPropertyPtr->OutputIndex;

				OpacityMultiply->B.Expression = GlobalOpacityExpression;
				OpacityMultiply->B.SetMask(1, 1, 0, 0, 0);
				OpacityMultiply->B.OutputIndex = 0;

				OpacityPropertyPtr->Expression = OpacityMultiply;
				OpacityPropertyPtr->SetMask(1, 1, 0, 0, 0);
			}
		}
	}

	/**
	 * Apply output processors.
	 */
	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(Pair.Key))
		{
			continue;
		}

		UMaterialFunctionInterface* OutputProcessor = Pair.Value->GetOutputProcessor();

		if (!OutputProcessor)
		{
			continue;
		}

		FExpressionInput* MaterialPropertyPtr = BuildState->GetMaterialProperty(Pair.Key);

		if (!MaterialPropertyPtr)
		{
			continue;
		}

		UMaterialExpressionMaterialFunctionCall* MFC = FDMMaterialFunctionLibrary::Get().MakeExpression(
			BuildState->GetDynamicMaterial(),
			OutputProcessor,
			UE_DM_NodeComment_Default
		);

		TArrayView<FExpressionInput*> Inputs = MFC->GetInputsView();

		if (Inputs.IsEmpty())
		{
			continue;
		}

		UMaterialExpression* LastPropertyExpression = MaterialPropertyPtr->Expression;

		if (!LastPropertyExpression)
		{
			LastPropertyExpression = Pair.Value->GetDefaultInput(BuildState);

			if (!LastPropertyExpression)
			{
				continue;
			}
		}

		LastPropertyExpression->ConnectExpression(Inputs[0], MaterialPropertyPtr->OutputIndex);
		MFC->ConnectExpression(MaterialPropertyPtr, 0);

		MaterialPropertyPtr->OutputIndex = 0;
	}

	State = EDMState::Idle;

	if (IsValid(MaterialModel) && IsValid(MaterialModel->DynamicMaterialInstance))
	{
		MaterialModel->DynamicMaterialInstance->OnMaterialBuilt(MaterialModel);
	}

	OnMaterialBuiltDelegate.Broadcast(MaterialModel);
}

void UDynamicMaterialModelEditorOnlyData::RequestMaterialBuild()
{
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		FDynamicMaterialEditorModule::Get().AddBuildRequest(this, /* Dirty Packages */ !GIsEditorLoadingPackage);
	}
}

void UDynamicMaterialModelEditorOnlyData::OnValueListUpdate()
{
	OnValueListUpdateDelegate.Broadcast(MaterialModel);
}

void UDynamicMaterialModelEditorOnlyData::LoadDeprecatedModelData_Base(bool bInCreateMaterialPackage, EBlendMode InBlendMode, 
	EDMMaterialShadingModel InShadingModel)
{
	bCreateMaterialPackage = bInCreateMaterialPackage;

	if (IsValid(MaterialModel->DynamicMaterial))
	{
		BlendMode = MaterialModel->DynamicMaterial->BlendMode;

		ShadingModel = MaterialModel->DynamicMaterial->GetShadingModels().HasShadingModel(EMaterialShadingModel::MSM_DefaultLit)
			? EDMMaterialShadingModel::DefaultLit
			: EDMMaterialShadingModel::Unlit;
	}
	else if (IsValid(MaterialModel->DynamicMaterialInstance))
	{
		BlendMode = MaterialModel->DynamicMaterialInstance->BlendMode;

		ShadingModel = MaterialModel->DynamicMaterialInstance->GetShadingModels().HasShadingModel(EMaterialShadingModel::MSM_DefaultLit)
			? EDMMaterialShadingModel::DefaultLit
			: EDMMaterialShadingModel::Unlit;
	}
	else
	{
		BlendMode = InBlendMode;
		ShadingModel = InShadingModel;
	}
}

void UDynamicMaterialModelEditorOnlyData::LoadDeprecatedModelData_Expressions(TArray<TObjectPtr<UMaterialExpression>>& InExpressions)
{
	// Expressions should be parented to the material.
	Expressions = InExpressions;
	InExpressions.Empty();
}

void UDynamicMaterialModelEditorOnlyData::LoadDeprecatedModelData_Properties(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InProperties)
{
	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UObject>>& Pair : InProperties)
	{
		UDMMaterialProperty* InProperty = Cast<UDMMaterialProperty>(Pair.Value);

		if (!InProperty)
		{
			continue;
		}

		if (const TObjectPtr<UDMMaterialProperty>* ThisPropertyPtr = Properties.Find(Pair.Key))
		{
			if (UDMMaterialProperty* ThisProperty = *ThisPropertyPtr)
			{
				ThisProperty->LoadDeprecatedModelData(InProperty);
			}
		}
	}

	InProperties.Empty();
}

void UDynamicMaterialModelEditorOnlyData::LoadDeprecatedModelData_Slots(TArray<TObjectPtr<UObject>>& InSlots)
{
	Slots.Empty();
	Slots.Reserve(InSlots.Num());

	for (TObjectPtr<UObject>& SlotObject : InSlots)
	{
		UDMMaterialSlot* Slot = Cast<UDMMaterialSlot>(SlotObject);

		if (!Slot)
		{
			continue;
		}

		Slots.Add(Slot);

		// Reparent from the model to the editor only data
		Slot->Rename(nullptr, this, UE::DynamicMaterial::RenameFlags);
	}

	InSlots.Empty();
}

void UDynamicMaterialModelEditorOnlyData::LoadDeprecatedModelData_PropertySlotMap(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InPropertySlotMap)
{
	PropertySlotMap.Empty();
	PropertySlotMap.Reserve(InPropertySlotMap.Num());

	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UObject>>& Pair : InPropertySlotMap)
	{
		UDMMaterialSlot* Slot = Cast<UDMMaterialSlot>(Pair.Value);

		if (!Slot)
		{
			continue;
		}

		PropertySlotMap.Add(Pair.Key, Slot);
	}

	InPropertySlotMap.Empty();
}

void UDynamicMaterialModelEditorOnlyData::LoadDeprecatedModelData(UDynamicMaterialModel* InMaterialModel)
{
	MaterialModel = InMaterialModel;
	check(InMaterialModel);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	LoadDeprecatedModelData_Base(MaterialModel->bCreateMaterialPackage, MaterialModel->BlendMode, MaterialModel->ShadingModel);
	LoadDeprecatedModelData_Expressions(MaterialModel->Expressions);
	LoadDeprecatedModelData_Properties(InMaterialModel->Properties);
	LoadDeprecatedModelData_Slots(InMaterialModel->Slots);
	LoadDeprecatedModelData_PropertySlotMap(InMaterialModel->PropertySlotMap);

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TSharedRef<FDMMaterialBuildState> UDynamicMaterialModelEditorOnlyData::CreateBuildState(UMaterial* InMaterialToBuild, bool bInDirtyAssets) const
{
	check(MaterialModel);
	check(InMaterialToBuild);

	return MakeShared<FDMMaterialBuildState>(InMaterialToBuild, MaterialModel, bInDirtyAssets);
}

bool UDynamicMaterialModelEditorOnlyData::NeedsWizard() const
{
	return ChannelListPreset == NAME_None;
}

void UDynamicMaterialModelEditorOnlyData::OnWizardComplete()
{
	if (UDynamicMaterialModel* MaterialModelLocal = MaterialModel.Get())
	{
		FDynamicMaterialEditorModule::Get().OnWizardComplete(MaterialModelLocal);
	}
}

void UDynamicMaterialModelEditorOnlyData::SetChannelListPreset(FName InPresetName)
{
	ChannelListPreset = InPresetName;

	const FDMMaterialChannelListPreset* Preset = GetDefault<UDynamicMaterialEditorSettings>()->ChannelPresets.Find(InPresetName);

	if (!Preset)
	{
		return;
	}

	for (uint8 PropertyIndex = static_cast<uint8>(EDMMaterialPropertyType::None) + 1;
		PropertyIndex < static_cast<uint8>(EDMMaterialPropertyType::Any);
		++PropertyIndex)
	{
		const EDMMaterialPropertyType Property = static_cast<EDMMaterialPropertyType>(PropertyIndex);

		if (Property == EDMMaterialPropertyType::EmissiveColor || Property == EDMMaterialPropertyType::OpacityMask)
		{
			continue;
		}

		if (Preset->IsPropertyEnabled(Property))
		{
			AddSlotForMaterialProperty(Property);
		}
		else
		{
			RemoveSlotForMaterialProperty(Property);
		}
	}

	SetBlendMode(Preset->DefaultBlendMode);
	SetShadingModel(Preset->DefaultShadingModel);
	SetPixelAnimationFlag(Preset->bDefaultAnimated);
	SetTwoSidedFlag(Preset->bDefaultTwoSided);
}

UDMMaterialComponent* UDynamicMaterialModelEditorOnlyData::GetSubComponentByPath(FDMComponentPath& InPath,
	const FDMComponentPathSegment& InPathSegment) const
{
	if (InPathSegment.GetToken() == SlotsPathToken)
	{
		int32 SlotIndex;

		if (InPathSegment.GetParameter(SlotIndex))
		{
			if (Slots.IsValidIndex(SlotIndex))
			{
				return Slots[SlotIndex]->GetComponentByPath(InPath);
			}
		}

		return nullptr;
	}

	if (InPathSegment.GetToken() == PropertiesPathToken)
	{
		FString PropertyStr;

		if (InPathSegment.GetParameter(PropertyStr))
		{
			UEnum* PropertyEnum = StaticEnum<EDMMaterialPropertyType>();
			int64 IntValue = PropertyEnum->GetValueByNameString(PropertyStr);

			if (IntValue != INDEX_NONE)
			{
				EDMMaterialPropertyType EnumValue = static_cast<EDMMaterialPropertyType>(IntValue);

				if (const TObjectPtr<UDMMaterialProperty>* PropertyPtr = Properties.Find(EnumValue))
				{
					return (*PropertyPtr)->GetComponentByPath(InPath);
				}
			}
		}
	}

	// Channels
	using namespace UE::DynamicMaterialEditor::Private;

	if (const EDMMaterialPropertyType* PropertyPtr = TokenToPropertyMap.Find(FString(InPathSegment.GetToken())))
	{
		if (const TObjectPtr<UDMMaterialSlot>* SlotPtr = PropertySlotMap.Find(*PropertyPtr))
		{
			return (*SlotPtr)->GetComponentByPath(InPath);
		}
	}

	return nullptr;
}

TSharedRef<IDMMaterialBuildStateInterface> UDynamicMaterialModelEditorOnlyData::CreateBuildStateInterface(UMaterial* InMaterialToBuild) const
{
	return CreateBuildState(InMaterialToBuild);
}

void UDynamicMaterialModelEditorOnlyData::DoBuild_Implementation(bool bInDirtyAssets)
{
	BuildMaterial(bInDirtyAssets);
}

void UDynamicMaterialModelEditorOnlyData::SwapSlotMaterialProperty(EDMMaterialPropertyType InPropertyFrom, EDMMaterialPropertyType InPropertyTo)
{
	UDMMaterialSlot* FromSlot = GetSlotForMaterialProperty(InPropertyFrom);
	
	if (!FromSlot)
	{
		return;
	}

	if (UDMMaterialSlot* ToSlot = GetSlotForMaterialProperty(InPropertyTo))
	{
		if (ToSlot == FromSlot)
		{
			return;
		}

		RemoveSlotForMaterialProperty(InPropertyFrom);
	}

	FromSlot->ChangeMaterialProperty(InPropertyFrom, InPropertyTo);
}

void UDynamicMaterialModelEditorOnlyData::EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType InPropertyFrom, EDMMaterialPropertyType InPropertyTo)
{
	if (UDMMaterialSlot* ToSlot = GetSlotForMaterialProperty(InPropertyTo))
	{
		if (UDMMaterialSlot* FromSlot = GetSlotForMaterialProperty(InPropertyFrom))
		{
			if (ToSlot != FromSlot)
			{
				RemoveSlotForMaterialProperty(InPropertyFrom);
			}
		}
	}
	else if (GetSlotForMaterialProperty(InPropertyFrom))
	{
		SwapSlotMaterialProperty(InPropertyFrom, InPropertyTo);
	}
	else
	{
		AddSlotForMaterialProperty(InPropertyTo);
	}
}

void UDynamicMaterialModelEditorOnlyData::SetDomain(TEnumAsByte<EMaterialDomain> InDomain)
{
	if (Domain == InDomain)
	{
		return;
	}

	Domain = InDomain;

	if (Domain == EMaterialDomain::MD_PostProcess)
	{
		const FDMUpdateGuard Guard;

		// Post process only supports emissive.
		for (uint8 PropertyIndex = static_cast<uint8>(EDMMaterialPropertyType::None) + 1;
			PropertyIndex < static_cast<uint8>(EDMMaterialPropertyType::Any);
			++PropertyIndex)
		{
			const EDMMaterialPropertyType Property = static_cast<EDMMaterialPropertyType>(PropertyIndex);

			switch (Property)
			{
				case EDMMaterialPropertyType::BaseColor:
				case EDMMaterialPropertyType::EmissiveColor:
					RemoveSlotForMaterialProperty(Property);
					continue;

				default:
					// Do nothing
					break;
			}			
		}

		EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType::BaseColor, EDMMaterialPropertyType::EmissiveColor);

		SetShadingModel(EDMMaterialShadingModel::Unlit);
		SetBlendMode(EBlendMode::BLEND_Opaque);
	}
	else if (Domain == EMaterialDomain::MD_DeferredDecal)
	{
		// Post process only supports basic types.
		for (uint8 PropertyIndex = static_cast<uint8>(EDMMaterialPropertyType::None) + 1;
			PropertyIndex < static_cast<uint8>(EDMMaterialPropertyType::Any);
			++PropertyIndex)
		{
			const EDMMaterialPropertyType Property = static_cast<EDMMaterialPropertyType>(PropertyIndex);

			switch (Property)
			{
				case EDMMaterialPropertyType::BaseColor:
				case EDMMaterialPropertyType::EmissiveColor:
				case EDMMaterialPropertyType::Opacity:
				case EDMMaterialPropertyType::OpacityMask:
					RemoveSlotForMaterialProperty(Property);
					continue;

				default:
					// Do nothing
					break;
			}			
		}

		EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType::EmissiveColor, EDMMaterialPropertyType::BaseColor);

		SetShadingModel(EDMMaterialShadingModel::DefaultLit);
		SetBlendMode(EBlendMode::BLEND_Translucent);
	}

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::SetBlendMode(TEnumAsByte<EBlendMode> InBlendMode)
{
	if (BlendMode == InBlendMode)
	{
		return;
	}

	BlendMode = InBlendMode;

	switch (InBlendMode)
	{
		case EBlendMode::BLEND_Opaque:
			SetPixelAnimationFlag(false);
			RemoveSlotForMaterialProperty(EDMMaterialPropertyType::Opacity);
			RemoveSlotForMaterialProperty(EDMMaterialPropertyType::OpacityMask);
			break;

		case EBlendMode::BLEND_Masked:
			SetPixelAnimationFlag(false);
			EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType::Opacity, EDMMaterialPropertyType::OpacityMask);
			break;

		case EBlendMode::BLEND_Translucent:
		case EBlendMode::BLEND_Additive:
		case EBlendMode::BLEND_Modulate:
			EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType::OpacityMask, EDMMaterialPropertyType::Opacity);
			break;
	}

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::SetShadingModel(EDMMaterialShadingModel InShadingModel)
{
	if (ShadingModel == InShadingModel)
	{
		return;
	}

	ShadingModel = InShadingModel;

	EDMMaterialPropertyType FromProperty;
	EDMMaterialPropertyType ToProperty;

	switch (ShadingModel)
	{
		case EDMMaterialShadingModel::Unlit:
			FromProperty = EDMMaterialPropertyType::BaseColor;
			ToProperty = EDMMaterialPropertyType::EmissiveColor;
			break;

		case EDMMaterialShadingModel::DefaultLit:
			FromProperty = EDMMaterialPropertyType::EmissiveColor;
			ToProperty = EDMMaterialPropertyType::BaseColor;
			break;

		default:
			return;
	}

	EnsureSwapSlotMaterialProperty(FromProperty, ToProperty);

	RequestMaterialBuild();
}

bool UDynamicMaterialModelEditorOnlyData::IsPixelAnimationFlagSet() const
{
	return bPixelAnimationFlag;
}

void UDynamicMaterialModelEditorOnlyData::SetPixelAnimationFlag(bool bInFlagValue)
{
	if (bPixelAnimationFlag == bInFlagValue)
	{
		return;
	}

	bPixelAnimationFlag = bInFlagValue;

	if (MaterialModel)
	{
		if (UMaterial* Material = MaterialModel->GetGeneratedMaterial())
		{
			if (GUndo)
			{
				Material->Modify();
			}

			Material->bHasPixelAnimation = bPixelAnimationFlag;
		}
	}

	RequestMaterialBuild();
}

bool UDynamicMaterialModelEditorOnlyData::IsTwoSidedFlagSet() const
{
	return bTwoSidedFlag;
}

void UDynamicMaterialModelEditorOnlyData::SetTwoSidedFlag(bool bInFlagValue)
{
	if (bTwoSidedFlag == bInFlagValue)
	{
		return;
	}

	bTwoSidedFlag = bInFlagValue;

	if (MaterialModel)
	{
		if (UMaterial* Material = MaterialModel->GetGeneratedMaterial())
		{
			if (GUndo)
			{
				Material->Modify();
			}

			Material->TwoSided = bTwoSidedFlag;
		}
	}

	RequestMaterialBuild();
}

FName UDynamicMaterialModelEditorOnlyData::GetChannelListPreset() const
{
	return ChannelListPreset;
}

void UDynamicMaterialModelEditorOnlyData::OpenMaterialEditor() const
{
	if (!IsValid(MaterialModel) || !IsValid(MaterialModel->DynamicMaterial))
	{
		return;
	}

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.OpenEditorForAssets({MaterialModel->DynamicMaterial});
}

TMap<EDMMaterialPropertyType, UDMMaterialProperty*> UDynamicMaterialModelEditorOnlyData::GetMaterialProperties() const
{
	TMap<EDMMaterialPropertyType, UDMMaterialProperty*> LocalProperties;

	LocalProperties.Reserve(Properties.Num());
	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		LocalProperties.Add(Pair.Key, Pair.Value.Get());
	}

	return LocalProperties;
}

UDMMaterialProperty* UDynamicMaterialModelEditorOnlyData::GetMaterialProperty(EDMMaterialPropertyType MaterialProperty) const
{
	TObjectPtr<UDMMaterialProperty> const* PropertyObjPtr = Properties.Find(MaterialProperty);

	if (PropertyObjPtr)
	{
		return *PropertyObjPtr;
	}

	return nullptr;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::GetSlot(int32 Index) const
{
	if (Slots.IsValidIndex(Index))
	{
		return Slots[Index];		
	}

	return nullptr;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::GetSlotForMaterialProperty(EDMMaterialPropertyType InType) const
{
	if (const TObjectPtr<UDMMaterialSlot>* SlotPtr = PropertySlotMap.Find(InType))
	{
		return *SlotPtr;
	}

	return nullptr;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::AddSlot()
{
	constexpr uint8 Start = static_cast<uint8>(EDMMaterialPropertyType::None) + 1;
	constexpr uint8 End = static_cast<uint8>(EDMMaterialPropertyType::Any);

	for (uint8 PropertyIndex = Start; PropertyIndex < End; ++PropertyIndex)
	{
		const EDMMaterialPropertyType Property = static_cast<EDMMaterialPropertyType>(PropertyIndex);

		if (GetSlotForMaterialProperty(Property))
		{
			continue;
		}

		return AddSlotForMaterialProperty(Property);
	}

	return nullptr;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::AddSlotForMaterialProperty(EDMMaterialPropertyType InType)
{
	if (InType == EDMMaterialPropertyType::BaseColor || InType == EDMMaterialPropertyType::EmissiveColor)
	{
		if (Domain == EMaterialDomain::MD_DeferredDecal)
		{
			InType = EDMMaterialPropertyType::EmissiveColor;
		}
		else
		{
			switch (ShadingModel)
			{
				case EDMMaterialShadingModel::DefaultLit:
					InType = EDMMaterialPropertyType::BaseColor;
					break;

				case EDMMaterialShadingModel::Unlit:
					InType = EDMMaterialPropertyType::EmissiveColor;
					break;

				default:
					checkNoEntry();
					break;
			}
		}
	}
	else if (Domain == EMaterialDomain::MD_PostProcess)
	{
		return nullptr;
	}
	else if (InType == EDMMaterialPropertyType::Opacity || InType == EDMMaterialPropertyType::OpacityMask)
	{
		switch (BlendMode)
		{
			case EBlendMode::BLEND_Translucent:
			case EBlendMode::BLEND_Additive:
			case EBlendMode::BLEND_Modulate:
				InType = EDMMaterialPropertyType::Opacity;
				break;

			case EBlendMode::BLEND_Masked:
				InType = EDMMaterialPropertyType::OpacityMask;
				break;

			case EBlendMode::BLEND_Opaque:
				return nullptr;

			default:
				checkNoEntry();
				break;
		}
	}

	if (Domain == EMaterialDomain::MD_DeferredDecal && InType != EDMMaterialPropertyType::EmissiveColor)
	{
		return nullptr;
	}

	if (UDMMaterialSlot* ExistingSlot = GetSlotForMaterialProperty(InType))
	{
		return ExistingSlot;
	}

	UDMMaterialSlot* NewSlot = NewObject<UDMMaterialSlot>(this, NAME_None, RF_Transactional);
	check(NewSlot);

	AssignMaterialPropertyToSlot(InType, NewSlot);

	NewSlot->SetIndex(Slots.Num());
	Slots.Add(NewSlot);
	NewSlot->SetComponentState(EDMComponentLifetimeState::Added);

	NewSlot->GetOnConnectorsUpdateDelegate().AddUObject(this, &UDynamicMaterialModelEditorOnlyData::OnSlotConnectorsUpdated);

	if (UDMMaterialProperty* Property = GetMaterialProperty(InType))
	{
		Property->OnSlotAdded(NewSlot);
	}

	NewSlot->Update(EDMUpdateType::Structure);

	OnSlotListUpdateDelegate.Broadcast(MaterialModel);

	RequestMaterialBuild();

	return NewSlot;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::RemoveSlot(int32 Index)
{
	UDMMaterialSlot* Slot = GetSlot(Index);
	
	if (!Slot)
	{
		return nullptr;
	}
	
	if (GUndo)
	{
		Slot->Modify(/* Always mark dirty */ false);
	}

	for (EDMMaterialPropertyType MaterialProperty : GetMaterialPropertiesForSlot(Slot))
	{
		UnassignMaterialProperty(MaterialProperty);
	}

	const EDMMaterialPropertyType* Key = PropertySlotMap.FindKey(Slot);

	if (Key)
	{
		PropertySlotMap.Remove(*Key);
	}

	const int32 SlotIndex = Slots.IndexOfByKey(Slot);

	if (SlotIndex != INDEX_NONE)
	{
		Slots.RemoveAtSwap(SlotIndex);
		
		if (Slots.IsValidIndex(SlotIndex))
		{
			if (GUndo)
			{
				Slots[SlotIndex]->Modify(/* Always mark dirty */ false);
			}

			Slots[SlotIndex]->SetIndex(SlotIndex);
		}
	}

	Slot->GetOnConnectorsUpdateDelegate().RemoveAll(this);
	Slot->SetComponentState(EDMComponentLifetimeState::Removed);

	RequestMaterialBuild();

	OnSlotListUpdateDelegate.Broadcast(MaterialModel);
	
	return Slot;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::RemoveSlotForMaterialProperty(EDMMaterialPropertyType InType)
{
	if (const TObjectPtr<UDMMaterialSlot>* SlotPtr = PropertySlotMap.Find(InType))
	{
		return RemoveSlot(Slots.IndexOfByKey(*SlotPtr));
	}

	return nullptr;
}

TArray<EDMMaterialPropertyType> UDynamicMaterialModelEditorOnlyData::GetMaterialPropertiesForSlot(UDMMaterialSlot* Slot) const
{
	TArray<EDMMaterialPropertyType> OutProperties;

	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialSlot>>& Pair : PropertySlotMap)
	{
		if (Pair.Value == Slot)
		{
			OutProperties.Add(Pair.Key);
		}
	}

	return OutProperties;
}

void UDynamicMaterialModelEditorOnlyData::AssignMaterialPropertyToSlot(EDMMaterialPropertyType Property, UDMMaterialSlot* Slot)
{
	if (!Slot)
	{
		UnassignMaterialProperty(Property);
		return;
	}

	check(Properties.Contains(Property));

	PropertySlotMap.FindOrAdd(Property) = Slot;
	Properties[Property]->ResetInputConnectionMap();
	Slot->GetOnPropertiesUpdateDelegate().Broadcast(Slot);

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::UnassignMaterialProperty(EDMMaterialPropertyType Property)
{
	TObjectPtr<UDMMaterialSlot>* SlotPtr = PropertySlotMap.Find(Property);
	
	if (!SlotPtr)
	{
		return;
	}

	PropertySlotMap.Remove(Property);
	(*SlotPtr)->GetOnPropertiesUpdateDelegate().Broadcast(*SlotPtr);

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged)
{
	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::PostLoad()
{
	Super::PostLoad();

	// Backwards compatibility change - materials were originally parented to this object instead of the model.
	if (IsValid(MaterialModel))
	{
		if (UMaterial* Material = MaterialModel->GetGeneratedMaterial())
		{
			if (Material->GetOuter() != MaterialModel)
			{
				Material->Rename(nullptr, MaterialModel, UE::DynamicMaterial::RenameFlags);
			}
		}
	}

	SetFlags(RF_Transactional);

	ReinitComponents();
}

void UDynamicMaterialModelEditorOnlyData::PostEditUndo()
{
	Super::PostEditUndo();

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::PostEditImport()
{
	Super::PostEditImport();

	PostEditorDuplicate();
	ReinitComponents();
	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	PostEditorDuplicate();
	ReinitComponents();
	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::OnValueUpdated(UDMMaterialValue* InValue, EDMUpdateType InUpdateType)
{
	check(InValue);

	if (InValue->GetMaterialModel() != MaterialModel)
	{
		return;
	}

	if (InUpdateType == EDMUpdateType::Value && IsValid(MaterialModel) && IsValid(MaterialModel->DynamicMaterialInstance))
	{
		InValue->SetMIDParameter(MaterialModel->DynamicMaterialInstance);
	}

	OnValueUpdateDelegate.Broadcast(MaterialModel, InValue);

	// Non-exported materials have their values update via settings parameters
	// Exported materials need to be rebuilt to update the main material.
	const bool bMaterialInDifferentPackage = MaterialModel->DynamicMaterial ? MaterialModel->DynamicMaterial->GetPackage() != GetPackage() : true;

	if (InUpdateType == EDMUpdateType::Structure || bMaterialInDifferentPackage)
	{
		RequestMaterialBuild();
	}
}

void UDynamicMaterialModelEditorOnlyData::OnTextureUVUpdated(UDMTextureUV* InTextureUV)
{
	check(InTextureUV);

	if (InTextureUV->GetMaterialModel() != MaterialModel)
	{
		return;
	}

	if (IsValid(MaterialModel) && IsValid(MaterialModel->DynamicMaterialInstance))
	{
		InTextureUV->SetMIDParameters(MaterialModel->DynamicMaterialInstance);
	}

	OnTextureUVUpdateDelegate.Broadcast(MaterialModel, InTextureUV);

	// Non-exported materials have their values update via settings parameters
	// Exported materials need to be rebuilt to update the main material.
	if (MaterialModel->DynamicMaterial && MaterialModel->DynamicMaterial->GetPackage() != GetPackage())
	{
		RequestMaterialBuild();
	}
}

void UDynamicMaterialModelEditorOnlyData::SaveEditor()
{
	UEditorLoadingAndSavingUtils::SavePackages({GetPackage()}, false);

	if (IsValid(MaterialModel) && IsValid(MaterialModel->DynamicMaterial))
	{
		if (MaterialModel->DynamicMaterial->GetPackage() != GetPackage())
		{
			UEditorLoadingAndSavingUtils::SavePackages({MaterialModel->DynamicMaterial->GetPackage()}, false);
		}
	}
}

FString UDynamicMaterialModelEditorOnlyData::GetMaterialAssetPath() const
{
	return FPaths::GetPath(GetPackage()->GetPathName());
}

FString UDynamicMaterialModelEditorOnlyData::GetMaterialAssetName() const
{
	return GetName() + "_Mat";
}

FString UDynamicMaterialModelEditorOnlyData::GetMaterialPackageName(const FString& MaterialBaseName) const
{
	return GetPackage()->GetName() + "_Mat";
}

void UDynamicMaterialModelEditorOnlyData::OnSlotConnectorsUpdated(UDMMaterialSlot* Slot)
{
	check(Slot);

	RequestMaterialBuild();
		
	TArray<EDMMaterialPropertyType> SlotProperties = GetMaterialPropertiesForSlot(Slot);

	for (EDMMaterialPropertyType Property : SlotProperties)
	{
		Properties[Property]->ResetInputConnectionMap();
	}
}

void UDynamicMaterialModelEditorOnlyData::GenerateOpacityExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState, UDMMaterialSlot* InFromSlot,
	EDMMaterialPropertyType InFromProperty, UMaterialExpression*& OutExpression, int32& OutOutputIndex, int32& OutOutputChannel) const
{
	const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = InFromSlot->GetLayers();
	OutExpression = nullptr;

	for (const TObjectPtr<UDMMaterialLayerObject>& Layer : SlotLayers)
	{
		// Although we are working with masks, if the base is disabled, this is handled by the GenerateExpressions
		// of the LayerBlend code (to multiply alpha together, instead of maxing it).
		if (Layer->GetMaterialProperty() != InFromProperty || !Layer->IsEnabled() || !Layer->IsStageEnabled(EDMMaterialLayerStage::Base))
		{
			continue;
		}

		UDMMaterialStage* BaseStage = Layer->GetStage(EDMMaterialLayerStage::Base);
		UDMMaterialStage* MaskStage = Layer->GetStage(EDMMaterialLayerStage::Mask);

		MaskStage->GenerateExpressions(InBuildState);
		UDMMaterialStageThroughputLayerBlend* LayerBlend = Cast<UDMMaterialStageThroughputLayerBlend>(MaskStage->GetSource());

		if (!LayerBlend)
		{
			continue;
		}

		UMaterialExpression* MaskOutputExpression;
		int32 MaskOutputIndex;
		int32 MaskOutputChannel;
		LayerBlend->GetMaskOutput(InBuildState, MaskOutputExpression, MaskOutputIndex, MaskOutputChannel);

		if (!MaskOutputExpression)
		{
			continue;
		}

		if (LayerBlend->UsePremultiplyAlpha())
		{
			if (UDMMaterialStageSource* Source = BaseStage->GetSource())
			{
				UMaterialExpression* LayerAlphaOutputExpression;
				int32 LayerAlphaOutputIndex;
				int32 LayerAlphaOutputChannel;

				Source->GetMaskAlphaBlendNode(InBuildState, LayerAlphaOutputExpression, LayerAlphaOutputIndex, LayerAlphaOutputChannel);

				if (LayerAlphaOutputExpression)
				{
					UMaterialExpressionMultiply* AlphaMultiply = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMultiply>(UE_DM_NodeComment_Default);

					AlphaMultiply->A.Expression = MaskOutputExpression;
					AlphaMultiply->A.OutputIndex = MaskOutputIndex;
					AlphaMultiply->A.Mask = 0;

					if (MaskOutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
					{
						AlphaMultiply->A.Mask = 1;
						AlphaMultiply->A.MaskR = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL);
						AlphaMultiply->A.MaskG = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL);
						AlphaMultiply->A.MaskB = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL);
						AlphaMultiply->A.MaskA = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL);
					}

					AlphaMultiply->B.Expression = LayerAlphaOutputExpression;
					AlphaMultiply->B.OutputIndex = LayerAlphaOutputIndex;
					AlphaMultiply->B.Mask = 0;

					if (LayerAlphaOutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
					{
						AlphaMultiply->B.Mask = 1;
						AlphaMultiply->B.MaskR = !!(LayerAlphaOutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL);
						AlphaMultiply->B.MaskG = !!(LayerAlphaOutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL);
						AlphaMultiply->B.MaskB = !!(LayerAlphaOutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL);
						AlphaMultiply->B.MaskA = !!(LayerAlphaOutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL);
					}

					MaskOutputExpression = AlphaMultiply;
					MaskOutputIndex = 0;
					MaskOutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
				}
			}
		}

		if (OutExpression == nullptr)
		{
			OutExpression = MaskOutputExpression;
			
			// The first output will use the node's output info.
			OutOutputIndex = MaskOutputIndex;
			OutOutputChannel = MaskOutputChannel;
			continue;
		}

		UMaterialExpressionMax* Max = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMax>(UE_DM_NodeComment_Default);
		check(Max);

		Max->A.Expression = OutExpression;
		Max->A.OutputIndex = OutOutputIndex;
		Max->A.Mask = 0;

		if (OutOutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
		{
			Max->A.Mask = 1;
			Max->A.MaskR = !!(OutOutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL);
			Max->A.MaskG = !!(OutOutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL);
			Max->A.MaskB = !!(OutOutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL);
			Max->A.MaskA = !!(OutOutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL);
		}

		Max->B.Expression = MaskOutputExpression;
		Max->B.OutputIndex = MaskOutputIndex;
		Max->B.Mask = 0;

		if (MaskOutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
		{
			Max->B.Mask = 1;
			Max->B.MaskR = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL);
			Max->B.MaskG = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL);
			Max->B.MaskB = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL);
			Max->B.MaskA = !!(MaskOutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL);
		}

		OutExpression = Max;

		// If we have to combine, it will use the Max node's output info
		OutOutputIndex = 0;
		OutOutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
	}
}

void UDynamicMaterialModelEditorOnlyData::ReinitComponents()
{
	for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
	{
		if (IsValid(Slots[SlotIdx]))
		{
			Slots[SlotIdx]->GetOnConnectorsUpdateDelegate().AddUObject(this, &UDynamicMaterialModelEditorOnlyData::OnSlotConnectorsUpdated);
		}
		else
		{
			Slots.RemoveAt(SlotIdx);
			--SlotIdx;
		}
	}
}

void UDynamicMaterialModelEditorOnlyData::PostEditorDuplicate()
{
	if (GUndo)
	{
		Modify();
	}

	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		if (UDMMaterialProperty* Property = Pair.Value.Get())
		{
			if (GUndo)
			{
				Property->Modify();
			}

			Property->PostEditorDuplicate(MaterialModel, nullptr);
		}
	}

	for (UDMMaterialSlot* Slot : Slots)
	{
		if (GUndo)
		{
			Slot->Modify();
		}

		Slot->PostEditorDuplicate(MaterialModel, nullptr);
	}

	PropertySlotMap.Empty();

	for (UDMMaterialSlot* Slot : Slots)
	{
		const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();

		for (const TObjectPtr<UDMMaterialLayerObject>& Layer : SlotLayers)
		{
			EDMMaterialPropertyType Property = Layer->GetMaterialProperty();
			TObjectPtr<UDMMaterialSlot>* SlotPtr = PropertySlotMap.Find(Property);

			if (!SlotPtr)
			{
				PropertySlotMap.Emplace(Property, Slot);
			}
			else
			{
				check(*SlotPtr == Slot);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
