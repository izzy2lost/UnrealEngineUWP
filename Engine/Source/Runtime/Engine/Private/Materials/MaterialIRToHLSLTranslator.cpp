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

#include <inttypes.h>

#if WITH_EDITOR

namespace IR = UE::MIR;

enum ENoOp { NoOp };
enum ENewLine { NewLine };
enum EEndOfStatement { EndOfStatement };
enum EOpenBrace { OpenBrace };
enum ECloseBrace { CloseBrace };
enum EIndentation { Indentation };

struct FHLSLPrinter
{
	FString Buffer;
	bool bFirstListItem = false;
	int32 Tabs = 0;

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
	
	FHLSLPrinter& operator<<(const FString& Text)
	{
		Buffer.Append(Text);
		return *this;
	}

	FHLSLPrinter& operator<<(int Value)
	{
		Buffer.Appendf(TEXT("%d"), Value);
		return *this;
	}

    FHLSLPrinter& operator<<(ENoOp)
	{
		return *this;
	}

    FHLSLPrinter& operator<<(ENewLine)
    {
		Buffer.AppendChar('\n');
		operator<<(Indentation);
        return *this;
    }

	FHLSLPrinter& operator<<(EIndentation)
	{
		for (int i = 0; i < Tabs; ++i)
		{
			Buffer.AppendChar('\t');
		}
		return *this;
	}

	FHLSLPrinter& operator<<(EEndOfStatement)
	{
		Buffer.AppendChar(';');
        *this << NewLine;
        return *this;
	}

    FHLSLPrinter& operator<<(EOpenBrace)
    {
        Buffer.Append("{");
        ++Tabs;
        *this << NewLine;
        return *this;
    }

    FHLSLPrinter& operator<<(ECloseBrace)
    {
        --Tabs;
        Buffer.LeftChopInline(1); // undo tab
        Buffer.AppendChar('}');
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

static bool IsFoldable(const IR::FInstruction* Instr)
{
	if (auto Branch = Instr->As<IR::FBranch>())
	{
		return !Branch->TrueBlock.Instructions && !Branch->FalseBlock.Instructions;
	}

	return true;
}

struct FTranslator : FMaterialIRToHLSLTranslation
{
	int32 NumLocals{};
	TMap<const IR::FInstruction*, FString> LocalToIdentifier;
	FHLSLPrinter Printer;
	FString PixelAttributesHLSL;
	FString EvaluateOtherMaterialAttributesHLSL;

	void GenerateHLSL()
	{
		Printer.Tabs = 1;
		Printer << Indentation;

		LowerBlock(Module->GetRootBlock());

		Printer << TEXT("PixelMaterialInputs.FrontMaterial = GetInitialisedSubstrateData()") << EndOfStatement;
		Printer << TEXT("PixelMaterialInputs.Subsurface = 0") << EndOfStatement;

		EvaluateOtherMaterialAttributesHLSL = MoveTemp(Printer.Buffer);

		for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
		{
			EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
			if (!UE::Utility::IsMaterialPropertyShared(Property))
			{
				continue;
			}
		
			check(FMaterialAttributeDefinitionMap::GetShaderFrequency(Property) == SF_Pixel);
			
			// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
			FString PropertyName = (Property == MP_SubsurfaceColor) ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Property);
			EMaterialValueType Type = (Property == MP_SubsurfaceColor) ? MCT_Float4 : FMaterialAttributeDefinitionMap::GetValueType(Property);
			check(PropertyName.Len() > 0);

			PixelAttributesHLSL.Appendf(TEXT("\t%s %s;\n"), GetHLSLTypeString(Type), *PropertyName);
		}
	}
	
	ENoOp LowerBlock(const IR::FBlock& Block)
	{
		int OldNumLocals = NumLocals;
        for (IR::FInstruction* Instr = Block.Instructions; Instr; Instr = Instr->Next)
		{
            if (Instr->NumUsers == 1 && IsFoldable(Instr))
			{
                continue;
            }
            
			if (Instr->NumUsers >= 1)
			{
                FString LocalStr = FString::Printf(TEXT("l%d"), NumLocals);
                ++NumLocals;

                Printer << InlineType(Instr->Type) << TEXT(" ") << LocalStr;

                LocalToIdentifier.Add(Instr, MoveTemp(LocalStr));
                if (IsFoldable(Instr))
				{
                    Printer << TEXT(" = ";)
                }
            }

			LowerInstruction(Instr);

            if (Printer.Buffer.EndsWith(TEXT("}")))
			{
                Printer << NewLine;
            }
            else
			{
                Printer << EndOfStatement;
            }
        }

        NumLocals = OldNumLocals;

		return NoOp;
	}

	ENoOp LowerInstruction(const IR::FInstruction* Instr)
	{
		switch (Instr->Kind)
		{
			case IR::VK_Dimensional:
			{
				const IR::FDimensional* Vector = static_cast<const IR::FDimensional*>(Instr);

				IR::FArithmeticTypePtr ArithType = Vector->Type->AsArithmetic();
				check(ArithType && ArithType->IsVector());

				Printer << ScalarKindToString(ArithType->ScalarKind) << ArithType->NumRows << TEXT("(");

				Printer.BeginList();
				for (IR::FValue* Component : Vector->GetComponents())
				{
					Printer.PrintListSeparator();
					LowerValue(Component);
				}

				Printer << TEXT(")");
				break;
			}

			case IR::VK_SetMaterialOutput:
			{
				auto Output = static_cast<const IR::FSetMaterialOutput*>(Instr);

				// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
				const FString& PropertyName = (Output->Property == MP_SubsurfaceColor) ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Output->Property);

				Printer << TEXT("PixelMaterialInputs.") << PropertyName << TEXT(" = ") << LowerValue(Output->Arg);

				break;
			}

			case IR::VK_BinaryOperator:
			{
				auto BinaryOperator = static_cast<const IR::FBinaryOperator*>(Instr);

				LowerValue(BinaryOperator->LhsArg);

				const TCHAR* OpString;
				switch (BinaryOperator->Operator)
				{
					case IR::BO_Add: OpString = TEXT(" + "); break;
					case IR::BO_Subtract: OpString = TEXT(" - "); break;
					case IR::BO_Multiply: OpString = TEXT(" * "); break;
					case IR::BO_Divide: OpString = TEXT(" / "); break;
					case IR::BO_Greater: OpString = TEXT(" > "); break;
					case IR::BO_Lower: OpString = TEXT(" < "); break;
					case IR::BO_Equals: OpString = TEXT(" == "); break;
					default: UE_MIR_UNREACHABLE();
				}
				Printer << OpString;

				LowerValue(BinaryOperator->RhsArg);

				break;
			}

			case IR::VK_Branch:
			{
				auto Branch = static_cast<const IR::FBranch*>(Instr);

				if (IsFoldable(Branch))
				{
					Printer << LowerValue(Branch->ConditionArg)
						<< TEXT(" ? ") << LowerValue(Branch->TrueArg)
						<< TEXT(" : ") << LowerValue(Branch->FalseArg);
				}
				else
				{
					Printer << EndOfStatement;
					Printer << TEXT("if (") << LowerValue(Branch->ConditionArg) << TEXT(")") << NewLine << OpenBrace;
					Printer << LowerBlock(Branch->TrueBlock);
					Printer << LocalToIdentifier[Instr] << " = " << LowerValue(Branch->TrueArg) << EndOfStatement;
					Printer << CloseBrace << NewLine;
					Printer << TEXT("else") << NewLine << OpenBrace;
					Printer << LowerBlock(Branch->FalseBlock);
					Printer << LocalToIdentifier[Instr] << " = " << LowerValue(Branch->FalseArg) << EndOfStatement;
					Printer << CloseBrace;
				}
				break;
			}
			
			case IR::VK_Subscript:
			{
				const IR::FSubscript* Subscript = static_cast<const IR::FSubscript*>(Instr);

				LowerValue(Subscript->Arg);

				if (IR::FArithmeticTypePtr ArgArithmeticType = Subscript->Arg->Type->AsVector())
				{
					const TCHAR* ComponentsStr[] = { TEXT(".x"), TEXT(".y"), TEXT(".z"), TEXT(".w") };
					check(Subscript->Index <= ArgArithmeticType->GetNumComponents());

					Printer << ComponentsStr[Subscript->Index];
				}

				break;
			}

			default:
				UE_MIR_UNREACHABLE();
		}

		return NoOp;
	}

	ENoOp LowerValue(const IR::FValue* InValue)
	{
		if (const IR::FInstruction* Instr = InValue->AsInstruction())
		{
			if (Instr->NumUsers <= 1 && IsFoldable(Instr))
			{
				Printer << LowerInstruction(Instr);
			}
			else
			{
				Printer << LocalToIdentifier[Instr];
			}

			return NoOp;
		}

		switch (InValue->Kind)
		{
			case IR::VK_Constant:
			{
				const IR::FConstant* Scalar = static_cast<const IR::FConstant*>(InValue);
				
				IR::FArithmeticTypePtr ArithType = Scalar->Type->AsArithmetic();
				check(ArithType && ArithType->IsScalar());

				switch (ArithType->ScalarKind)
				{
					case IR::SK_Bool:  Printer.Buffer.Append(Scalar->Boolean ? TEXT("true") : TEXT("false")); break;
					case IR::SK_Int:   Printer.Buffer.Appendf(TEXT("%") PRId64, Scalar->Integer); break;
					case IR::SK_Float: Printer.Buffer.Appendf(TEXT("%.5ff"), Scalar->Float); break;
				}

				break;
			}

			default:
			{
				UE_MIR_UNREACHABLE();
			}
		}

		return NoOp;
	}

	ENoOp InlineType(const IR::FType* Type)
	{
		if (auto ArithmeticType = Type->AsArithmetic())
		{
			switch (ArithmeticType->ScalarKind)
			{
				case IR::SK_Bool:	Printer << TEXT("bool"); break;
				case IR::SK_Int: 	Printer << TEXT("int"); break;
				case IR::SK_Float:	Printer << TEXT("float"); break;
			}

			if (ArithmeticType->NumRows > 1)
			{
				Printer << ArithmeticType->NumRows;
			}
			
			if (ArithmeticType->NumColumns > 1)
			{
				Printer << TEXT("x") << ArithmeticType->NumColumns;
			}
		}
		else
		{
			UE_MIR_UNREACHABLE();
		}

		return NoOp;
	}

	void SetMaterialParameters(TMap<FString, FString>& Params)
	{
		auto SetParamInt = [&] (const TCHAR* InParamName, int InValue)
		{
			Params.Add(InParamName, FString::Printf(TEXT("%d"), InValue));
		};
		
		auto SetParamReturnFloat = [&] (const TCHAR* InParamName, float InValue)
		{
			Params.Add(InParamName, FString::Printf(TEXT("\treturn %.5f"), InValue));
		};

		Params.Add(TEXT("pixel_material_inputs"), MoveTemp(PixelAttributesHLSL));
		Params.Add(TEXT("calc_pixel_material_inputs_initial_calculations"), EvaluateOtherMaterialAttributesHLSL);
		Params.Add(TEXT("calc_pixel_material_inputs_analytic_derivatives_initial"), MoveTemp(EvaluateOtherMaterialAttributesHLSL));
		Params.Add(TEXT("material_declarations"), TEXT("struct FMaterialAttributes {};"));
		SetParamInt(TEXT("num_material_texcoords_vertex"), 0);
		SetParamInt(TEXT("num_material_texcoords"), 0);
		SetParamInt(TEXT("num_custom_vertex_interpolators"), 0);
		SetParamInt(TEXT("num_tex_coord_interpolators"), 0);

		SetParamReturnFloat(TEXT("get_material_emissive_for_cs"), 0.f);
		SetParamReturnFloat(TEXT("get_material_translucency_directional_lighting_intensity"), Material->GetTranslucencyDirectionalLightingIntensity());
		SetParamReturnFloat(TEXT("get_material_translucent_shadow_density_scale"), Material->GetTranslucentShadowDensityScale());
		SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_density_scale"), Material->GetTranslucentSelfShadowDensityScale());
		SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_second_density_scale"), Material->GetTranslucentSelfShadowSecondDensityScale());
		SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_second_opacity"), Material->GetTranslucentSelfShadowSecondOpacity());
		SetParamReturnFloat(TEXT("get_material_translucent_backscattering_exponent"), Material->GetTranslucentBackscatteringExponent());

		FLinearColor Extinction = Material->GetTranslucentMultipleScatteringExtinction();
		Params.Add(TEXT("get_material_translucent_multiple_scattering_extinction"), FString::Printf(TEXT("\treturn MaterialFloat3(%.5f, %.5f, %.5f)"), Extinction.R, Extinction.G, Extinction.B));

		SetParamReturnFloat(TEXT("get_material_opacity_mask_clip_value"), Material->GetOpacityMaskClipValue());
		Params.Add(TEXT("get_material_world_position_offset_raw"), TEXT("\treturn 0; // todo"));
		Params.Add(TEXT("get_material_previous_world_position_offset_raw"), TEXT("\treturn 0; // todo"));
	
		// CustomData0/1 are named ClearCoat/ClearCoatRoughness
		Params.Add(TEXT("get_material_custom_data0"), TEXT("\treturn 1.0f; // todo"));
		Params.Add(TEXT("get_material_custom_data1"), TEXT("\treturn 0.1f; // todo"));

		FString EvaluateMaterialDeclaration;
		EvaluateMaterialDeclaration.Append(TEXT("void EvaluateVertexMaterialAttributes(in out FMaterialVertexParameters Parameters)\n{\n"));
		EvaluateMaterialDeclaration.Append(TEXT("\n}\n"));
		Params.Add(TEXT("evaluate_material_attributes"), EvaluateMaterialDeclaration);
	}

	void GetShaderCompilerEnvironment(FShaderCompilerEnvironment& OutEnvironment)
	{
		const FMaterialCompilationOutput& CompilationOutput = Module->GetCompilationOutput();
		EShaderPlatform ShaderPlatform = Module->GetShaderPlatform();

		OutEnvironment.TargetPlatform = TargetPlatform;
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
		OutEnvironment.SetDefine(TEXT("USES_DISTORTION"), Material->IsDistorted());
		OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_FOGGING"), Material->ShouldApplyFogging());
		OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_CLOUD_FOGGING"), Material->ShouldApplyCloudFogging());
		OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SKY"), Material->IsSky());
		OutEnvironment.SetDefine(TEXT("MATERIAL_COMPUTE_FOG_PER_PIXEL"), Material->ComputeFogPerPixel());
		OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), false);
		OutEnvironment.SetDefine(TEXT("MATERIAL_USES_ANISOTROPY"), false);
		OutEnvironment.SetDefine(TEXT("MATERIAL_NEURAL_POST_PROCESS"), (CompilationOutput.bUsedWithNeuralNetworks || Material->IsUsedWithNeuralNetworks()) && Material->IsPostProcessMaterial());
		OutEnvironment.SetDefine(TEXT("NUM_VIRTUALTEXTURE_SAMPLES"), 0);
		OutEnvironment.SetDefine(TEXT("MATERIAL_VIRTUALTEXTURE_FEEDBACK"), false);
		OutEnvironment.SetDefine(TEXT("IS_MATERIAL_SHADER"), true);

		FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
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
			OutEnvironment.SetDefine(GetShadingModelParameterName(MSM_Unlit), true);
			NumActiveShadingModels += 1;
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

		if (Material->GetMaterialDomain() == MD_Volume)
		{
			TArray<const UMaterialExpressionVolumetricAdvancedMaterialOutput*> VolumetricAdvancedExpressions;
			Material->GetMaterialInterface()->GetMaterial()->GetAllExpressionsOfType(VolumetricAdvancedExpressions);
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
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_OVERRIDE_AMBIENT_OCCLUSION"), Material->HasAmbientOcclusionConnected());
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GROUND_CONTRIBUTION"), VolumetricAdvancedNode->bGroundContribution);
			}
		}

		OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SUBSTRATE"), false);
		OutEnvironment.SetDefine(TEXT("DUAL_SOURCE_COLOR_BLENDING_ENABLED"), false);
		OutEnvironment.SetDefine(TEXT("TEXTURE_SAMPLE_DEBUG"), false);
	}
};

void FMaterialIRToHLSLTranslation::Run(TMap<FString, FString>& OutParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutParameters.Empty();

	FTranslator Translator{ *this };
	Translator.GenerateHLSL();
	Translator.SetMaterialParameters(OutParameters);
	Translator.GetShaderCompilerEnvironment(OutEnvironment);
}

#endif // #if WITH_EDITOR
