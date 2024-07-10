// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/DynamicMaterialModelEditorOnlyData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialSubStage.h"
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
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "CoreGlobals.h"
#include "DMComponentPath.h"
#include "DMDefs.h"
#include "DMTextureSet.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Misc/Guid.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyDataVersion.h"
#include "UObject/Package.h"
#include "Utils/DMUtils.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerModel"

const FString UDynamicMaterialModelEditorOnlyData::SlotsPathToken               = FString(TEXT("Slots"));
const FString UDynamicMaterialModelEditorOnlyData::BaseColorSlotPathToken       = FString(TEXT("BaseColor"));
const FString UDynamicMaterialModelEditorOnlyData::EmissiveSlotPathToken        = FString(TEXT("Emissive"));
const FString UDynamicMaterialModelEditorOnlyData::OpacitySlotPathToken         = FString(TEXT("Opacity"));
const FString UDynamicMaterialModelEditorOnlyData::RoughnessPathToken           = FString(TEXT("Roughness"));
const FString UDynamicMaterialModelEditorOnlyData::SpecularPathToken            = FString(TEXT("Specular"));
const FString UDynamicMaterialModelEditorOnlyData::MetallicPathToken            = FString(TEXT("Metallic"));
const FString UDynamicMaterialModelEditorOnlyData::NormalPathToken              = FString(TEXT("Normal"));
const FString UDynamicMaterialModelEditorOnlyData::PixelDepthOffsetPathToken    = FString(TEXT("PDO"));
const FString UDynamicMaterialModelEditorOnlyData::WorldPositionOffsetPathToken = FString(TEXT("WPO"));
const FString UDynamicMaterialModelEditorOnlyData::AmbientOcclusionPathToken    = FString(TEXT("AO"));
const FString UDynamicMaterialModelEditorOnlyData::AnisotropyPathToken          = FString(TEXT("Anisotropy"));
const FString UDynamicMaterialModelEditorOnlyData::RefractionPathToken          = FString(TEXT("Refraction"));
const FString UDynamicMaterialModelEditorOnlyData::TangentPathToken             = FString(TEXT("Tangent"));
const FString UDynamicMaterialModelEditorOnlyData::Custom1PathToken             = FString(TEXT("Custom1"));
const FString UDynamicMaterialModelEditorOnlyData::Custom2PathToken             = FString(TEXT("Custom2"));
const FString UDynamicMaterialModelEditorOnlyData::Custom3PathToken             = FString(TEXT("Custom3"));
const FString UDynamicMaterialModelEditorOnlyData::Custom4PathToken             = FString(TEXT("Custom4"));
const FString UDynamicMaterialModelEditorOnlyData::PropertiesPathToken          = FString(TEXT("Properties"));

namespace UE::DynamicMaterialEditor::Private
{
	const TMap<FString, EDMMaterialPropertyType> TokenToPropertyMap = {
		{UDynamicMaterialModelEditorOnlyData::BaseColorSlotPathToken,       EDMMaterialPropertyType::BaseColor},
		{UDynamicMaterialModelEditorOnlyData::EmissiveSlotPathToken,        EDMMaterialPropertyType::EmissiveColor},
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

const FName UDynamicMaterialModelEditorOnlyData::AlphaValueName = TEXT("AlphaValue");

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

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(UDynamicMaterialModelBase* InModelBase)
{
	if (InModelBase)
	{
		return Get(InModelBase->ResolveMaterialModel());
	}

	return nullptr;
}

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(const TWeakObjectPtr<UDynamicMaterialModelBase>& InModelBaseWeak)
{
	return Get(InModelBaseWeak.Get());
}

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(UDynamicMaterialModel* InModel)
{
	if (InModel)
	{
		return Get(InModel->GetEditorOnlyData());
	}

	return nullptr;
}

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(const TWeakObjectPtr<UDynamicMaterialModel>& InModelWeak)
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

UDynamicMaterialModelEditorOnlyData* UDynamicMaterialModelEditorOnlyData::Get(UDynamicMaterialInstance* InInstance)
{
	if (InInstance)
	{
		return Get(InInstance->GetMaterialModel());
	}

	return nullptr;
}

UDynamicMaterialModelEditorOnlyData::UDynamicMaterialModelEditorOnlyData()
	: State(EDMState::Idle)
	, Domain(EMaterialDomain::MD_Surface)
	, BlendMode(EBlendMode::BLEND_Opaque)
	, ShadingModel(EDMMaterialShadingModel::DefaultLit)
	, bPixelAnimationFlag(false)
	, bTwoSidedFlag(true)
	, ChannelListPreset(NAME_None)
{
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

void UDynamicMaterialModelEditorOnlyData::AssignPropertyAlphaValues()
{
	Properties[EDMMaterialPropertyType::Opacity            ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOpacityValueName));
	Properties[EDMMaterialPropertyType::OpacityMask        ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOpacityValueName));
	Properties[EDMMaterialPropertyType::Metallic           ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalMetallicValueName));
	Properties[EDMMaterialPropertyType::Specular           ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalSpecularValueName));
	Properties[EDMMaterialPropertyType::Roughness          ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalRoughnessValueName));
	Properties[EDMMaterialPropertyType::Normal             ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalNormalValueName));
	Properties[EDMMaterialPropertyType::Anisotropy         ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalAnisotropyValueName));
	Properties[EDMMaterialPropertyType::WorldPositionOffset]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalWorldPositionOffsetValueName));
	Properties[EDMMaterialPropertyType::AmbientOcclusion   ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalAmbientOcclusionValueName));
	Properties[EDMMaterialPropertyType::Refraction         ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalRefractionValueName));
	Properties[EDMMaterialPropertyType::PixelDepthOffset   ]->AddComponent(AlphaValueName, MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalPixelDepthOffsetValueName));
}

TArray<FName> UDynamicMaterialModelEditorOnlyData::GetPresetOptions() const
{
	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	if (!Settings)
	{
		return {ChannelListPreset};
	}

	TArray<FName> PresetNames;
	PresetNames.Reserve(Settings->MaterialChannelPresets.Num());

	for (const FDMMaterialChannelListPreset& Preset : Settings->MaterialChannelPresets)
	{
		PresetNames.Add(Preset.Name);
	}

	return PresetNames;
}

void UDynamicMaterialModelEditorOnlyData::OnChannelListPresetChanged()
{
	const FDMMaterialChannelListPreset* Preset = GetDefault<UDynamicMaterialEditorSettings>()->GetPresetByName(ChannelListPreset);

	if (!Preset)
	{
		return;
	}

	EnsurePresetSlots();

	SetBlendMode(Preset->DefaultBlendMode);
	SetShadingModel(Preset->DefaultShadingModel);
	SetPixelAnimationFlag(Preset->bDefaultAnimated);
	SetTwoSidedFlag(Preset->bDefaultTwoSided);
}

void UDynamicMaterialModelEditorOnlyData::EnsurePresetSlots()
{
	const FDMMaterialChannelListPreset* Preset = GetDefault<UDynamicMaterialEditorSettings>()->GetPresetByName(ChannelListPreset);

	if (!Preset)
	{
		return;
	}

	ForEachMaterialPropertyType(
		[this, Preset](EDMMaterialPropertyType InProperty)
		{
			if (InProperty == EDMMaterialPropertyType::OpacityMask)
			{
				return EDMIterationResult::Continue;
			}

			if (Preset->IsPropertyEnabled(InProperty))
			{
				AddSlotForMaterialProperty(InProperty);
			}
			else
			{
				RemoveSlotForMaterialProperty(InProperty);
			}

			return EDMIterationResult::Continue;
		});
}

void UDynamicMaterialModelEditorOnlyData::OnDomainChanged()
{
	if (Domain == EMaterialDomain::MD_PostProcess)
	{
		const FDMUpdateGuard Guard;

		// Make sure we have the correct slots, then adjust for domains.
		EnsurePresetSlots();

		// Post process only supports emissive.
		UDMMaterialSlot* BaseColorSlot = GetSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor);
		UDMMaterialSlot* EmissiveSlot = GetSlotForMaterialProperty(EDMMaterialPropertyType::EmissiveColor);

		if (!EmissiveSlot)
		{
			if (BaseColorSlot)
			{
				EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType::BaseColor, EDMMaterialPropertyType::EmissiveColor);
			}
			else
			{
				AddSlotForMaterialProperty(EDMMaterialPropertyType::EmissiveColor);
			}
		}

		SetShadingModel(EDMMaterialShadingModel::Unlit);
		SetBlendMode(EBlendMode::BLEND_Opaque);
	}
	else if (Domain == EMaterialDomain::MD_DeferredDecal)
	{
		SetShadingModel(EDMMaterialShadingModel::DefaultLit);
		SetBlendMode(EBlendMode::BLEND_Translucent);
	}

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::OnBlendModeChanged()
{
	EnsurePresetSlots();

	switch (BlendMode)
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

void UDynamicMaterialModelEditorOnlyData::OnShadingModelChanged()
{
	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::OnPixelAnimationFlagChanged()
{
	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::OnTwoSidedFlagChanged()
{
	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::Initialize()
{
	if (!Slots.IsEmpty())
	{
		return;
	}

	AssignPropertyAlphaValues();

	// Will choose appropriate type between BaseColor and EmissiveColor
	AddSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor);
}

UMaterial* UDynamicMaterialModelEditorOnlyData::GetGeneratedMaterial() const
{
	return IsValid(MaterialModel) ? MaterialModel->DynamicMaterial : nullptr;
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
			RF_DuplicateTransient | RF_TextExportTransient | RF_Public,
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

	/**
	 * Process slots to build base material inputs.
	 */
	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		if (!Pair.Value->IsMaterialPin())
		{
			continue;
		}

		Pair.Value->GenerateExpressions(BuildState);

		// Global opacity is handled at later
		if (Pair.Key != EDMMaterialPropertyType::Opacity && Pair.Key != EDMMaterialPropertyType::OpacityMask)
		{
			Pair.Value->AddAlphaMultiplier(BuildState);
		}
	}

	if (Domain != EMaterialDomain::MD_PostProcess)
	{
		/**
		 * Generate opacity input based on base/emissive if it doesn't already have an input.
		 */
		EDMMaterialPropertyType GenerateOpacityInput = EDMMaterialPropertyType::None;

		if (BlendMode == EBlendMode::BLEND_Masked)
		{
			if (GetSlotForMaterialProperty(EDMMaterialPropertyType::OpacityMask) == nullptr)
			{
				GenerateOpacityInput = EDMMaterialPropertyType::OpacityMask;
			}
		}
		else if (BlendMode != EBlendMode::BLEND_Opaque)
		{
			if (GetSlotForMaterialProperty(EDMMaterialPropertyType::Opacity) == nullptr)
			{
				GenerateOpacityInput = EDMMaterialPropertyType::Opacity;
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
				UDMMaterialProperty::GenerateOpacityExpressions(BuildState, OpacitySlot, OpacityProperty, OpacityOutputNode, OutputIndex, OutputChannel);

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
		 * Apply global opacity slider after automatic opacity generation
		 */
		UDMMaterialProperty* OpacityProperty = nullptr;

		if (BlendMode == BLEND_Masked)
		{
			OpacityProperty = Properties.FindRef(EDMMaterialPropertyType::OpacityMask);
		}
		else if (BlendMode != BLEND_Opaque)
		{
			OpacityProperty = Properties.FindRef(EDMMaterialPropertyType::Opacity);
		}

		if (OpacityProperty != nullptr)
		{
			OpacityProperty->AddAlphaMultiplier(BuildState);
		}
	}

	/**
	 * Apply output processors.
	 */
	for (const TPair<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>>& Pair : Properties)
	{
		if (!Pair.Value->IsMaterialPin())
		{
			continue;
		}

		Pair.Value->AddOutputProcessor(BuildState);
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
		FDynamicMaterialEditorModule::Get().AddBuildRequest(this, /* Dirty Packages */ !UE::GetIsEditorLoadingPackage());
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

	TSharedRef<FDMMaterialBuildState> BuildState = MakeShared<FDMMaterialBuildState>(InMaterialToBuild, MaterialModel, bInDirtyAssets);

	/**
	 * Add global UV parameters
	 */
	if (UDMMaterialValue* GlobalOffsetValue = MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOffsetValueName))
	{
		GlobalOffsetValue->GenerateExpression(BuildState);
		BuildState->SetGlobalExpression(UDynamicMaterialModel::GlobalOffsetValueName, BuildState->GetLastValueExpression(GlobalOffsetValue));
	}

	if (UDMMaterialValue* GlobalTilingValue = MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalTilingValueName))
	{
		GlobalTilingValue->GenerateExpression(BuildState);
		BuildState->SetGlobalExpression(UDynamicMaterialModel::GlobalTilingValueName, BuildState->GetLastValueExpression(GlobalTilingValue));
	}

	if (UDMMaterialValue* GlobalRotationValue = MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalRotationValueName))
	{
		GlobalRotationValue->GenerateExpression(BuildState);
		BuildState->SetGlobalExpression(UDynamicMaterialModel::GlobalRotationValueName, BuildState->GetLastValueExpression(GlobalRotationValue));
	}

	return BuildState;
}

bool UDynamicMaterialModelEditorOnlyData::AddTextureSet(UDMTextureSet* InTextureSet, bool bInReplaceSlots)
{
	bool bMadeChange = false;

	for (const TPair<EDMTextureSetMaterialProperty, FDMMaterialTexture>& MaterialTexture : InTextureSet->GetTextures())
	{
		const EDMMaterialPropertyType PropertyType = FDMUtils::TextureSetMaterialPropertyToMaterialPropertyType(MaterialTexture.Key);

		if (PropertyType == EDMMaterialPropertyType::None)
		{
			continue;
		}

		UDMMaterialSlot* Slot = GetSlotForMaterialProperty(PropertyType);

		if (!Slot)
		{
			continue;
		}

		UTexture* Texture = MaterialTexture.Value.Texture.LoadSynchronous();

		if (!Texture)
		{
			continue;
		}

		if (GUndo)
		{
			Slot->Modify();
		}

		UDMMaterialLayerObject* Layer = nullptr;

		{
			const FDMUpdateGuard Guard;

			Layer = Slot->AddDefaultLayer(PropertyType);

			if (!ensure(Layer))
			{
				continue;
			}

			bMadeChange = true;

			UDMMaterialStage* Stage = Layer->GetStage(EDMMaterialLayerStage::Base);

			if (!ensure(Stage))
			{
				continue;
			}

			UDMMaterialStageInputExpression* NewExpression = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage,
				UDMMaterialStageExpressionTextureSample::StaticClass(),
				UDMMaterialStageBlend::InputB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
				0,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);

			if (!ensure(NewExpression))
			{
				continue;
			}

			UDMMaterialSubStage* SubStage = NewExpression->GetSubStage();

			if (!ensure(SubStage))
			{
				continue;
			}

			UDMMaterialStageInputValue* InputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				SubStage,
				0,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
				EDMValueType::VT_Texture,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);

			if (ensure(InputValue))
			{
				UDMMaterialValueTexture* InputTexture = Cast<UDMMaterialValueTexture>(InputValue->GetValue());

				if (ensure(InputTexture))
				{

					InputTexture->SetValue(Texture);
				}
			}

			if (UDMMaterialStageBlend* Blend = Cast<UDMMaterialStageBlend>(Stage->GetSource()))
			{
				EAvaColorChannel AvaColorChannel = EAvaColorChannel::None;

				if (EnumHasAnyFlags(MaterialTexture.Value.TextureChannel, EDMTextureChannelMask::Red))
				{
					AvaColorChannel |= EAvaColorChannel::Red;
				}

				if (EnumHasAnyFlags(MaterialTexture.Value.TextureChannel, EDMTextureChannelMask::Green))
				{
					AvaColorChannel |= EAvaColorChannel::Green;
				}

				if (EnumHasAnyFlags(MaterialTexture.Value.TextureChannel, EDMTextureChannelMask::Blue))
				{
					AvaColorChannel |= EAvaColorChannel::Blue;
				}

				if (EnumHasAnyFlags(MaterialTexture.Value.TextureChannel, EDMTextureChannelMask::Alpha))
				{
					AvaColorChannel |= EAvaColorChannel::Alpha;
				}

				if (AvaColorChannel != EAvaColorChannel::RGBA)
				{
					Blend->SetBaseChannelOverride(AvaColorChannel);
				}				
			}

			if (bInReplaceSlots)
			{
				for (int32 Index = Slot->GetLayers().Num() - 1; Index >= 0; --Index)
				{
					UDMMaterialLayerObject* LayerIter = Slot->GetLayer(Index);

					if (!LayerIter || LayerIter->GetStage(EDMMaterialLayerStage::Base) == Stage)
					{
						continue;
					}

					Slot->RemoveLayer(LayerIter);
				}
			}
		}

		Layer->Update(EDMUpdateType::Structure);
	}

	return bMadeChange;
}

bool UDynamicMaterialModelEditorOnlyData::NeedsWizard() const
{
	return ChannelListPreset.IsNone();
}

void UDynamicMaterialModelEditorOnlyData::OnWizardComplete()
{
	if (UDynamicMaterialModel* MaterialModelLocal = MaterialModel.Get())
	{
		FDynamicMaterialEditorModule::Get().OnWizardComplete(MaterialModelLocal);
	}
}

void UDynamicMaterialModelEditorOnlyData::ForEachMaterialPropertyType(TFunctionRef<EDMIterationResult(EDMMaterialPropertyType InType)> InCallable,
	EDMMaterialPropertyType InStart, EDMMaterialPropertyType InEnd)
{
	for (uint8 PropertyIndex = static_cast<uint8>(InStart); PropertyIndex <= static_cast<uint8>(InEnd); ++PropertyIndex)
	{
		const EDMMaterialPropertyType Property = static_cast<EDMMaterialPropertyType>(PropertyIndex);

		if (InCallable(Property) == EDMIterationResult::Break)
		{
			break;
		}
	}
}

void UDynamicMaterialModelEditorOnlyData::SetChannelListPreset(FName InPresetName)
{
	ChannelListPreset = InPresetName;

	OnChannelListPresetChanged();
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

void UDynamicMaterialModelEditorOnlyData::SetPropertyComponent(EDMMaterialPropertyType InPropertyType, FName InComponentName, UDMMaterialComponent* InComponent)
{
	if (TObjectPtr<UDMMaterialProperty>* Property = Properties.Find(InPropertyType))
	{
		(*Property)->AddComponent(InComponentName, InComponent);
	}
}

UDMMaterialComponent* UDynamicMaterialModelEditorOnlyData::GetSubComponentByPath(FDMComponentPath& InPath) const
{
	if (InPath.IsLeaf())
	{
		// This is not a component
		return nullptr;
	}

	// Fetches the first component of the path and removes it from the path
	const FDMComponentPathSegment FirstComponent = InPath.GetFirstSegment();

	if (UDMMaterialComponent* SubComponent = GetSubComponentByPath(InPath, FirstComponent))
	{
		return SubComponent->GetComponentByPath(InPath);
	}

	return nullptr;
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

	OnDomainChanged();
}

void UDynamicMaterialModelEditorOnlyData::SetBlendMode(TEnumAsByte<EBlendMode> InBlendMode)
{
	if (BlendMode == InBlendMode)
	{
		return;
	}

	BlendMode = InBlendMode;

	OnBlendModeChanged();
}

void UDynamicMaterialModelEditorOnlyData::SetShadingModel(EDMMaterialShadingModel InShadingModel)
{
	if (ShadingModel == InShadingModel)
	{
		return;
	}

	ShadingModel = InShadingModel;

	OnShadingModelChanged();
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

	OnPixelAnimationFlagChanged();
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

	OnTwoSidedFlagChanged();
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
	UDMMaterialSlot* NewSlot = nullptr;

	ForEachMaterialPropertyType(
		[this, &NewSlot](EDMMaterialPropertyType InProperty)
		{
			if (GetSlotForMaterialProperty(InProperty))
			{
				return EDMIterationResult::Continue;
			}

			NewSlot = AddSlotForMaterialProperty(InProperty);
			return EDMIterationResult::Break;
		});

	return NewSlot;
}

UDMMaterialSlot* UDynamicMaterialModelEditorOnlyData::AddSlotForMaterialProperty(EDMMaterialPropertyType InType)
{
	if (InType == EDMMaterialPropertyType::EmissiveColor && Domain == EMaterialDomain::MD_PostProcess)
	{
		return nullptr;
	}
	if (InType == EDMMaterialPropertyType::Opacity || InType == EDMMaterialPropertyType::OpacityMask)
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

	if (UDMMaterialSlot* ExistingSlot = GetSlotForMaterialProperty(InType))
	{
		return ExistingSlot;
	}

	// BaseColor and Emissive are mutually exclusive so if something tries to add one of them, the other must
	// be checked. If it is found, it is converted and returned. The same goes for Opacity and OpacityMask.
	switch (InType)
	{
		case EDMMaterialPropertyType::Opacity:
			if (UDMMaterialSlot* ExistingSlot = GetSlotForMaterialProperty(EDMMaterialPropertyType::OpacityMask))
			{
				SwapSlotMaterialProperty(EDMMaterialPropertyType::OpacityMask, EDMMaterialPropertyType::Opacity);
				return ExistingSlot;
			}
			break;

		case EDMMaterialPropertyType::OpacityMask:
			if (UDMMaterialSlot* ExistingSlot = GetSlotForMaterialProperty(EDMMaterialPropertyType::Opacity))
			{
				SwapSlotMaterialProperty(EDMMaterialPropertyType::Opacity, EDMMaterialPropertyType::OpacityMask);
				return ExistingSlot;
			}
			break;
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

TArray<EDMMaterialPropertyType> UDynamicMaterialModelEditorOnlyData::GetMaterialPropertiesForSlot(const UDMMaterialSlot* Slot) const
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
	Slot->OnPropertiesUpdated();

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
	(*SlotPtr)->OnPropertiesUpdated();

	RequestMaterialBuild();
}

void UDynamicMaterialModelEditorOnlyData::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged)
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

	AssignPropertyAlphaValues();

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

void UDynamicMaterialModelEditorOnlyData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeChainProperty(PropertyChangedEvent);

	const FName Property = PropertyChangedEvent.GetMemberPropertyName();

	if (Property == GET_MEMBER_NAME_CHECKED(ThisClass, ChannelListPreset))
	{
		OnChannelListPresetChanged();
	}
	else if (Property == GET_MEMBER_NAME_CHECKED(ThisClass, Domain))
	{
		OnDomainChanged();
	}
	else if (Property == GET_MEMBER_NAME_CHECKED(ThisClass, BlendMode))
	{
		OnBlendModeChanged();
	}
	else if (Property == GET_MEMBER_NAME_CHECKED(ThisClass, ShadingModel))
	{
		OnShadingModelChanged();
	}
	else if (Property == GET_MEMBER_NAME_CHECKED(ThisClass, bPixelAnimationFlag))
	{
		OnPixelAnimationFlagChanged();
	}
	else if (Property == GET_MEMBER_NAME_CHECKED(ThisClass, bTwoSidedFlag))
	{
		OnTwoSidedFlagChanged();
	}
}

void UDynamicMaterialModelEditorOnlyData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FDynamicMaterialModelEditorOnlyDataVersion::GUID);

	Super::Serialize(Ar);

	const int32 Version = Ar.CustomVer(FDynamicMaterialModelEditorOnlyDataVersion::GUID);

	// The default blend mode was changed from translucent and to opaque. If we have a
	// transparent channel, let's set it back to translucent.
	if (Version < FDynamicMaterialModelEditorOnlyDataVersion::GlobalValueRename)
	{
		if (ChannelListPreset.IsNone())
		{
			// Try to guess from the available slots.
			if (GetSlotForMaterialProperty(EDMMaterialPropertyType::Opacity))
			{
				ChannelListPreset = "Translucent";
				BlendMode = BLEND_Translucent;
				ShadingModel = EDMMaterialShadingModel::Unlit;
			}
			else if (GetSlotForMaterialProperty(EDMMaterialPropertyType::OpacityMask))
			{
				ChannelListPreset = "Translucent";
				BlendMode = BLEND_Masked;
				ShadingModel = EDMMaterialShadingModel::Unlit;
			}
			if (GetSlotForMaterialProperty(EDMMaterialPropertyType::EmissiveColor))
			{
				ChannelListPreset = "Translucent";
				BlendMode = BLEND_Translucent;
				ShadingModel = EDMMaterialShadingModel::Unlit;
			}
			// Else let's stay on opaque
			else
			{
				BlendMode = BLEND_Opaque;
				ShadingModel = EDMMaterialShadingModel::DefaultLit;
				ChannelListPreset = "Opaque";
			}
		}
	}
}

void UDynamicMaterialModelEditorOnlyData::OnValueUpdated(UDMMaterialValue* InValue, EDMUpdateType InUpdateType)
{
	check(InValue);

	// Non-exported materials have their values update via settings parameters
	// Exported materials need to be rebuilt to update the main material.
	const bool bMaterialInDifferentPackage = MaterialModel->DynamicMaterial ? MaterialModel->DynamicMaterial->GetPackage() != GetPackage() : true;

	if (EnumHasAnyFlags(InUpdateType, EDMUpdateType::Structure) || bMaterialInDifferentPackage)
	{
		RequestMaterialBuild();
	}
}

void UDynamicMaterialModelEditorOnlyData::OnTextureUVUpdated(UDMTextureUV* InTextureUV)
{
	check(InTextureUV);

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
