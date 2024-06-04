// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRToHLSLTranslator.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIR.h"
#include "MaterialIRUtility.h"

#include "ShaderCore.h"
#include "MaterialShared.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "RenderUtils.h"

#if WITH_EDITOR

namespace IR = MaterialIR;

struct FHLSLPrinter
{
	FString& Buffer;
	bool bFirstListItem = false;

	template <int N, typename... Types>
	void Printf(const TCHAR (&Format)[N], Types... Args)
	{
		Buffer.Appendf(Format, Args...);
	}

	FHLSLPrinter& operator<<(const TCHAR* Text)
	{
 		Buffer.Append(Text);
		return *this;
	}

	FHLSLPrinter& operator<<(int32 Value)
	{
 		Buffer.Appendf(TEXT("%d"), Value);
		return *this;
	}

	FHLSLPrinter& operator<<(uint32 Value)
	{
 		Buffer.Appendf(TEXT("%u"), Value);
		return *this;
	}

	FHLSLPrinter& operator<<(float Value)
	{
 		Buffer.Appendf(TEXT("%.5ff"), Value);
		return *this;
	}

	void BeginList()
	{
		bFirstListItem = true;
	}

	void PrintListSeparator()
	{
		if (!bFirstListItem)
		{
			Buffer.Append(TEXT(", "));
		}
		bFirstListItem = false;
	}
};

static const TCHAR* GetHLSLTypeString(EMaterialValueType Type)
{
	switch (Type)
	{
	case MCT_Float1: return TEXT("MaterialFloat");
	case MCT_Float2: return TEXT("MaterialFloat2");
	case MCT_Float3: return TEXT("MaterialFloat3");
	case MCT_Float4: return TEXT("MaterialFloat4");
	case MCT_Float: return TEXT("MaterialFloat");
	case MCT_Texture2D: return TEXT("texture2D");
	case MCT_TextureCube: return TEXT("textureCube");
	case MCT_Texture2DArray: return TEXT("texture2DArray");
	case MCT_VolumeTexture: return TEXT("volumeTexture");
	case MCT_StaticBool: return TEXT("static bool");
	case MCT_Bool:  return TEXT("bool");
	case MCT_MaterialAttributes: return TEXT("FMaterialAttributes");
	case MCT_TextureExternal: return TEXT("TextureExternal");
	case MCT_TextureVirtual: return TEXT("TextureVirtual");
	case MCT_VTPageTableResult: return TEXT("VTPageTableResult");
	case MCT_ShadingModel: return TEXT("uint");
	case MCT_UInt: return TEXT("uint");
	case MCT_UInt1: return TEXT("uint");
	case MCT_UInt2: return TEXT("uint2");
	case MCT_UInt3: return TEXT("uint3");
	case MCT_UInt4: return TEXT("uint4");
	case MCT_Substrate: return TEXT("FSubstrateData");
	case MCT_TextureCollection: return TEXT("FResourceCollection");
	default: return TEXT("unknown");
	};
}

static const TCHAR* GetShadingModelParameterName(EMaterialShadingModel InModel)
{
	switch (InModel)
	{
		case MSM_Unlit: return TEXT("MATERIAL_SHADINGMODEL_UNLIT");
		case MSM_DefaultLit: return TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT");
		case MSM_Subsurface: return TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE");
		case MSM_PreintegratedSkin: return TEXT("MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN");
		case MSM_ClearCoat: return TEXT("MATERIAL_SHADINGMODEL_CLEAR_COAT");
		case MSM_SubsurfaceProfile: return TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE");
		case MSM_TwoSidedFoliage: return TEXT("MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE");
		case MSM_Hair: return TEXT("MATERIAL_SHADINGMODEL_HAIR");
		case MSM_Cloth: return TEXT("MATERIAL_SHADINGMODEL_CLOTH");
		case MSM_Eye: return TEXT("MATERIAL_SHADINGMODEL_EYE");
		case MSM_SingleLayerWater: return TEXT("MATERIAL_SHADINGMODEL_SINGLELAYERWATER");
		case MSM_ThinTranslucent: return TEXT("MATERIAL_SHADINGMODEL_THIN_TRANSLUCENT");
		default: UE_MIR_UNREACHABLE();
	}
}

UE_MIR_BEGIN_PRIVATE(FMaterialIRToHLSLTranslator);

void GenerateHLSL(const FMaterial& InMaterial, const FMaterialIRModule& InModule, FParametersMap& InParams)
{
	FString PixelAttributes;
	FString EvaluateOtherMaterialAttributesHLSL;
	FHLSLPrinter Printer{ EvaluateOtherMaterialAttributesHLSL };

 	for (const IR::FSetMaterialOutputInstr* Output : InModule.GetOutputs())
	{
		LowerValue(Printer, Output);
	}

	EvaluateOtherMaterialAttributesHLSL.Append(TEXT("\tPixelMaterialInputs.FrontMaterial = GetInitialisedSubstrateData();\n"));
	EvaluateOtherMaterialAttributesHLSL.Append(TEXT("\tPixelMaterialInputs.Subsurface = 0;\n"));

	for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
	{
		EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
		if (!Utility::IsMaterialPropertyShared(Property))
		{
			continue;
		}
		
		check(FMaterialAttributeDefinitionMap::GetShaderFrequency(Property) == SF_Pixel);
			
		// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
		FString PropertyName = (Property == MP_SubsurfaceColor) ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Property);
		EMaterialValueType Type = (Property == MP_SubsurfaceColor) ? MCT_Float4 : FMaterialAttributeDefinitionMap::GetValueType(Property);
		check(PropertyName.Len() > 0);

		PixelAttributes.Appendf(TEXT("\t%s %s;\n"), GetHLSLTypeString(Type), *PropertyName);
	}
	
	InParams.Add(TEXT("pixel_material_inputs"), MoveTemp(PixelAttributes));

	InParams.Add(TEXT("calc_pixel_material_inputs_normal"), TEXT("\tPixelMaterialInputs.Normal = MaterialFloat3(0.00000000, 0.00000000, 1.00000000);"));
	InParams.Add(TEXT("calc_pixel_material_inputs_other_inputs"), EvaluateOtherMaterialAttributesHLSL);

	InParams.Add(TEXT("calc_pixel_material_inputs_analytic_derivatives_normal"), TEXT("\tPixelMaterialInputs.Normal = MaterialFloat3(0.00000000, 0.00000000, 1.00000000);"));
	InParams.Add(TEXT("calc_pixel_material_inputs_analytic_derivatives_other_inputs"), MoveTemp(EvaluateOtherMaterialAttributesHLSL));
	
	InParams.Add(TEXT("material_declarations"), TEXT("struct FMaterialAttributes {};"));
	
	auto SetParamInt = [&] (const TCHAR* InParamName, int InValue)
	{
		InParams.Add(InParamName, FString::Printf(TEXT("%d"), InValue));
	};

	SetParamInt(TEXT("num_material_texcoords_vertex"), 0);
	SetParamInt(TEXT("num_material_texcoords"), 0);
	SetParamInt(TEXT("num_custom_vertex_interpolators"), 0);
	SetParamInt(TEXT("num_tex_coord_interpolators"), 0);
}

void SetMaterialParameters(const FMaterial& InMaterial, FParametersMap& InParams)
{
	auto SetParamReturnFloat = [&] (const TCHAR* InParamName, float InValue)
	{
		InParams.Add(InParamName, FString::Printf(TEXT("\treturn %.5f"), InValue));
	};

	SetParamReturnFloat(TEXT("get_material_emissive_for_cs"), 0.f);
	SetParamReturnFloat(TEXT("get_material_translucency_directional_lighting_intensity"), InMaterial.GetTranslucencyDirectionalLightingIntensity());
	SetParamReturnFloat(TEXT("get_material_translucent_shadow_density_scale"), InMaterial.GetTranslucentShadowDensityScale());
	SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_density_scale"), InMaterial.GetTranslucentSelfShadowDensityScale());
	SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_second_density_scale"), InMaterial.GetTranslucentSelfShadowSecondDensityScale());
	SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_second_opacity"), InMaterial.GetTranslucentSelfShadowSecondOpacity());
	SetParamReturnFloat(TEXT("get_material_translucent_backscattering_exponent"), InMaterial.GetTranslucentBackscatteringExponent());

	FLinearColor Extinction = InMaterial.GetTranslucentMultipleScatteringExtinction();
	InParams.Add(TEXT("get_material_translucent_multiple_scattering_extinction"), FString::Printf(TEXT("\treturn MaterialFloat3(%.5f, %.5f, %.5f)"), Extinction.R, Extinction.G, Extinction.B));

	SetParamReturnFloat(TEXT("get_material_opacity_mask_clip_value"), InMaterial.GetOpacityMaskClipValue());
	InParams.Add(TEXT("get_material_world_position_offset_raw"), TEXT("\treturn 0; // todo"));
	InParams.Add(TEXT("get_material_previous_world_position_offset_raw"), TEXT("\treturn 0; // todo"));
	
	// CustomData0/1 are named ClearCoat/ClearCoatRoughness
	InParams.Add(TEXT("get_material_custom_data0"), TEXT("\treturn 1.0f; // todo"));
	InParams.Add(TEXT("get_material_custom_data1"), TEXT("\treturn 0.1f; // todo"));

	FString EvaluateMaterialDeclaration;
	EvaluateMaterialDeclaration.Append(TEXT("void EvaluateVertexMaterialAttributes(in out FMaterialVertexParameters Parameters)\n{\n"));
	EvaluateMaterialDeclaration.Append(TEXT("\n}\n"));
	InParams.Add(TEXT("evaluate_material_attributes"), EvaluateMaterialDeclaration);
}

void GetShaderCompilerEnvironment(const FMaterial& InMaterial, const FMaterialIRModule& InModule, FShaderCompilerEnvironment& OutEnvironment)
{
	EShaderPlatform ShaderPlatform = InModule.GetShaderPlatform();
	const FMaterialCompilationOutput& CompilationOutput = InModule.GetCompilationOutput();

	OutEnvironment.TargetPlatform = InModule.GetTargetPlatform();
	OutEnvironment.SetDefine(TEXT("ENABLE_NEW_HLSL_GENERATOR"), 1);
	OutEnvironment.SetDefine(TEXT("MATERIAL_ATMOSPHERIC_FOG"), false);
	OutEnvironment.SetDefine(TEXT("MATERIAL_SKY_ATMOSPHERE"), false);
	OutEnvironment.SetDefine(TEXT("INTERPOLATE_VERTEX_COLOR"), false);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_COLOR"), false);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_LOCAL_TO_WORLD"), false);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_WORLD_TO_LOCAL"), false);
	OutEnvironment.SetDefine(TEXT("NEEDS_PER_INSTANCE_RANDOM_PS"), false);
	OutEnvironment.SetDefine(TEXT("USES_TRANSFORM_VECTOR"), false);
	OutEnvironment.SetDefine(TEXT("WANT_PIXEL_DEPTH_OFFSET"), CompilationOutput.bUsesPixelDepthOffset);
	OutEnvironment.SetDefineAndCompileArgument(TEXT("USES_WORLD_POSITION_OFFSET"), (bool)CompilationOutput.bUsesWorldPositionOffset);
	OutEnvironment.SetDefineAndCompileArgument(TEXT("USES_DISPLACEMENT"), false);
	OutEnvironment.SetDefine(TEXT("USES_EMISSIVE_COLOR"), false);
	OutEnvironment.SetDefine(TEXT("USES_DISTORTION"), InMaterial.IsDistorted());
	OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_FOGGING"), InMaterial.ShouldApplyFogging());
	OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_CLOUD_FOGGING"), InMaterial.ShouldApplyCloudFogging());
	OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SKY"), InMaterial.IsSky());
	OutEnvironment.SetDefine(TEXT("MATERIAL_COMPUTE_FOG_PER_PIXEL"), InMaterial.ComputeFogPerPixel());
	OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), false);
	OutEnvironment.SetDefine(TEXT("MATERIAL_USES_ANISOTROPY"), false);
	OutEnvironment.SetDefine(TEXT("MATERIAL_NEURAL_POST_PROCESS"), (CompilationOutput.bUsedWithNeuralNetworks || InMaterial.IsUsedWithNeuralNetworks()) && InMaterial.IsPostProcessMaterial());
	OutEnvironment.SetDefine(TEXT("NUM_VIRTUALTEXTURE_SAMPLES"), 0);
	OutEnvironment.SetDefine(TEXT("MATERIAL_VIRTUALTEXTURE_FEEDBACK"), false);
	OutEnvironment.SetDefine(TEXT("IS_MATERIAL_SHADER"), true);

	FMaterialShadingModelField ShadingModels = InMaterial.GetShadingModels();
	ensure(ShadingModels.IsValid());

	int32 NumActiveShadingModels = 0;
	if (ShadingModels.IsLit())
	{
		// This is to have platforms use the simple single layer water shading similar to mobile: no dynamic lights, only sun and sky, no distortion, no colored transmittance on background, no custom depth read.
		const bool bSingleLayerWaterUsesSimpleShading = FDataDrivenShaderPlatformInfo::GetWaterUsesSimpleForwardShading(ShaderPlatform) && IsForwardShadingEnabled(ShaderPlatform);

		for (int i = 0; i < MSM_NUM; ++i)
		{
			EMaterialShadingModel Model = (EMaterialShadingModel)i;
			if (Model == MSM_Strata || !ShadingModels.HasShadingModel(Model))
			{
				continue;
			}

			if (Model == MSM_SingleLayerWater && !FDataDrivenShaderPlatformInfo::GetRequiresDisableForwardLocalLights(ShaderPlatform))
			{
				continue;
			}

			if (Model == MSM_SingleLayerWater && bSingleLayerWaterUsesSimpleShading)
			{
				// Value must match SINGLE_LAYER_WATER_SHADING_QUALITY_MOBILE_WITH_DEPTH_TEXTURE in SingleLayerWaterCommon.ush!
				OutEnvironment.SetDefine(TEXT("SINGLE_LAYER_WATER_SHADING_QUALITY"), true);
			}

			OutEnvironment.SetDefine(GetShadingModelParameterName(Model), true);
			NumActiveShadingModels += 1;
		}
	}
	else
	{
		// Unlit shading model can only exist by itself
		OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), true);
		OutEnvironment.SetDefine(GetShadingModelParameterName(MSM_Unlit), true);
	}

	if (NumActiveShadingModels == 1)
	{
		OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), true);
	}
	else if (!ensure(NumActiveShadingModels > 0))
	{
		UE_LOG(LogMaterial, Warning, TEXT("Unknown material shading model(s). Setting to MSM_DefaultLit"));
		OutEnvironment.SetDefine(GetShadingModelParameterName(MSM_DefaultLit), true);
	}

	static IConsoleVariable* CVarLWCIsEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MaterialEditor.LWCEnabled"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_LWC_ENABLED"), CVarLWCIsEnabled->GetInt());
	OutEnvironment.SetDefine(TEXT("WSVECTOR_IS_TILEOFFSET"), true);
	OutEnvironment.SetDefine(TEXT("WSVECTOR_IS_DOUBLEFLOAT"), false);

	if (InMaterial.GetMaterialDomain() == MD_Volume)
	{
		TArray<const UMaterialExpressionVolumetricAdvancedMaterialOutput*> VolumetricAdvancedExpressions;
		InMaterial.GetMaterialInterface()->GetMaterial()->GetAllExpressionsOfType(VolumetricAdvancedExpressions);
		if (VolumetricAdvancedExpressions.Num() > 0)
		{
			if (VolumetricAdvancedExpressions.Num() > 1)
			{
				UE_LOG(LogMaterial, Fatal, TEXT("Only a single UMaterialExpressionVolumetricAdvancedMaterialOutput node is supported."));
			}

			const UMaterialExpressionVolumetricAdvancedMaterialOutput* VolumetricAdvancedNode = VolumetricAdvancedExpressions[0];
			const TCHAR* Param = VolumetricAdvancedNode->GetEvaluatePhaseOncePerSample() ? TEXT("MATERIAL_VOLUMETRIC_ADVANCED_PHASE_PERSAMPLE") : TEXT("MATERIAL_VOLUMETRIC_ADVANCED_PHASE_PERPIXEL");
			OutEnvironment.SetDefine(Param, true);

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED"), true);
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GRAYSCALE_MATERIAL"), VolumetricAdvancedNode->bGrayScaleMaterial);
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_RAYMARCH_VOLUME_SHADOW"), VolumetricAdvancedNode->bRayMarchVolumeShadow);
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_CLAMP_MULTISCATTERING_CONTRIBUTION"), VolumetricAdvancedNode->bClampMultiScatteringContribution);
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_MULTISCATTERING_OCTAVE_COUNT"), VolumetricAdvancedNode->GetMultiScatteringApproximationOctaveCount());
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_CONSERVATIVE_DENSITY"), VolumetricAdvancedNode->ConservativeDensity.IsConnected());
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_OVERRIDE_AMBIENT_OCCLUSION"), InMaterial.HasAmbientOcclusionConnected());
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GROUND_CONTRIBUTION"), VolumetricAdvancedNode->bGroundContribution);
		}
	}

	OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SUBSTRATE"), false);
	OutEnvironment.SetDefine(TEXT("DUAL_SOURCE_COLOR_BLENDING_ENABLED"), false);
	OutEnvironment.SetDefine(TEXT("TEXTURE_SAMPLE_DEBUG"), false);
}

void LowerValue(FHLSLPrinter& Printer, const IR::FValue* InValue)
{
	if (const IR::FScalarValue* Scalar = InValue->Cast<IR::FScalarValue>())
	{
		IR::FArithmeticTypePtr ArithType = Scalar->Type->ToArithmetic();
		check(ArithType && ArithType->IsScalar());

		switch (ArithType->ScalarKind)
		{
			case IR::SK_Bool: Printer << Scalar->Boolean; break;
			case IR::SK_Int: Printer << Scalar->Integer; break;
			case IR::SK_Float: Printer << Scalar->Float; break;
		}
	}
	else if (const IR::FVectorValue* Vector = InValue->Cast<IR::FVectorValue>())
	{
		IR::FArithmeticTypePtr ArithType = Vector->Type->ToArithmetic();
		check(ArithType && ArithType->IsVector());

		Printer << ScalarKindToString(ArithType->ScalarKind) << ArithType->NumRows << TEXT("(");

		Printer.BeginList();
		for (IR::FValuePtr Component : Vector->GetComponents())
		{
			Printer.PrintListSeparator();
			LowerValue(Printer, Component);
		}

		Printer << TEXT(")");
	}
	else if (const IR::FSetMaterialOutputInstr* Output = InValue->Cast<IR::FSetMaterialOutputInstr>())
	{
		// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
		const FString& PropertyName = (Output->Property == MP_SubsurfaceColor) ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Output->Property);

		Printer.Printf(TEXT("\tPixelMaterialInputs.%s = "), *PropertyName);
		
		LowerValue(Printer, Output->ArgValue);

		Printer << TEXT(";\n");
	}
	else
	{
		UE_MIR_UNREACHABLE();
	}
}

UE_MIR_END_PRIVATE(); //FMaterialIRToHLSLTranslator

FMaterialIRToHLSLTranslator::FMaterialIRToHLSLTranslator()
{
}

void FMaterialIRToHLSLTranslator::Translate(const FMaterial& InMaterial, const FMaterialIRModule& InModule, FParametersMap& OutParametersMap, FShaderCompilerEnvironment& OutEnvironment)
{
	OutParametersMap.Empty();

	AsPrivate()->GenerateHLSL(InMaterial, InModule, OutParametersMap);
	AsPrivate()->SetMaterialParameters(InMaterial, OutParametersMap);
	AsPrivate()->GetShaderCompilerEnvironment(InMaterial, InModule, OutEnvironment);
}

#endif // #if WITH_EDITOR
