// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Curves/CurveLinearColorAtlas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "MaterialDomain.h"
#include "MaterialExpressionIO.h"
#include "MaterialHLSLGenerator.h"
#include "MaterialHLSLTree.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAbsorptionMediumMaterialOutput.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAntialiasedTextureMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionArccosine.h"
#include "Materials/MaterialExpressionArccosineFast.h"
#include "Materials/MaterialExpressionArcsine.h"
#include "Materials/MaterialExpressionArcsineFast.h"
#include "Materials/MaterialExpressionArctangent.h"
#include "Materials/MaterialExpressionArctangent2.h"
#include "Materials/MaterialExpressionArctangent2Fast.h"
#include "Materials/MaterialExpressionArctangentFast.h"
#include "Materials/MaterialExpressionAtmosphericLightColor.h"
#include "Materials/MaterialExpressionAtmosphericLightVector.h"
#include "Materials/MaterialExpressionBentNormalCustomOutput.h"
#include "Materials/MaterialExpressionBinaryOp.h"
#include "Materials/MaterialExpressionBlackBody.h"
#include "Materials/MaterialExpressionBlendMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionChannelMaskParameter.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionCloudLayer.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstantBiasScale.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDataDrivenShaderPlatformInfoSwitch.h"
#include "Materials/MaterialExpressionDBufferTexture.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionDecalColor.h"
#include "Materials/MaterialExpressionDecalDerivative.h"
#include "Materials/MaterialExpressionDecalLifetimeOpacity.h"
#include "Materials/MaterialExpressionDecalMipmapLevel.h"
#include "Materials/MaterialExpressionDeltaTime.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionDepthOfFieldFunction.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDistanceCullFade.h"
#include "Materials/MaterialExpressionDistanceFieldApproxAO.h"
#include "Materials/MaterialExpressionDistanceFieldGradient.h"
#include "Materials/MaterialExpressionDistanceFieldsRenderingSwitch.h"
#include "Materials/MaterialExpressionDistanceToNearestSurface.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionDoubleVectorParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialExpressionExecEnd.h"
#include "Materials/MaterialExpressionExponential.h"
#include "Materials/MaterialExpressionExponential2.h"
#include "Materials/MaterialExpressionEyeAdaptation.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionAtmosphericFogColor.h"
#include "Materials/MaterialExpressionFontSample.h"
#include "Materials/MaterialExpressionForLoop.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionGenericConstant.h"
#include "Materials/MaterialExpressionGetLocal.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionGIReplace.h"
#include "Materials/MaterialExpressionHairAttributes.h"
#include "Materials/MaterialExpressionHairColor.h"
#include "Materials/MaterialExpressionHsvToRgb.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionIfThenElse.h"
#include "Materials/MaterialExpressionInverseLinearInterpolate.h"
#include "Materials/MaterialExpressionIsOrthographic.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLightmapUVs.h"
#include "Materials/MaterialExpressionLightVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionLightmassReplace.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionLogarithm.h"
#include "Materials/MaterialExpressionLogarithm10.h"
#include "Materials/MaterialExpressionLogarithm2.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMapARPassthroughCameraUV.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMaterialProxyReplace.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionModulo.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionNaniteReplace.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionNeuralPostProcessNode.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionObjectLocalBounds.h"
#include "Materials/MaterialExpressionBounds.h"
#include "Materials/MaterialExpressionObjectOrientation.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleDirection.h"
#include "Materials/MaterialExpressionParticleMacroUV.h"
#include "Materials/MaterialExpressionParticleMotionBlurFade.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"
#include "Materials/MaterialExpressionParticleRadius.h"
#include "Materials/MaterialExpressionParticleRandom.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionParticleSize.h"
#include "Materials/MaterialExpressionParticleSpeed.h"
#include "Materials/MaterialExpressionParticleSubUVProperties.h"
#include "Materials/MaterialExpressionPathTracingBufferTexture.h"
#include "Materials/MaterialExpressionPathTracingQualitySwitch.h"
#include "Materials/MaterialExpressionPathTracingRayTypeSwitch.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionPrecomputedAOMask.h"
#include "Materials/MaterialExpressionPreSkinnedLocalBounds.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionPreviousFrameSwitch.h"
#include "Materials/MaterialExpressionSamplePhysicsField.h"
#include "Materials/MaterialExpressionSphericalParticleOpacity.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionRayTracingQualitySwitch.h"
#include "Materials/MaterialExpressionReflectionCapturePassSwitch.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionRgbToHsv.h"
#include "Materials/MaterialExpressionRotateAboutAxis.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionRound.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureReplace.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSampleParameter.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionSceneDepthWithoutWater.h"
#include "Materials/MaterialExpressionSceneTexelSize.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSetLocal.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionShaderStageSwitch.h"
#include "Materials/MaterialExpressionShadingModel.h"
#include "Materials/MaterialExpressionShadingPathSwitch.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionSign.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightDirection.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightIlluminance.h"
#include "Materials/MaterialExpressionSkyAtmosphereViewLuminance.h"
#include "Materials/MaterialExpressionSkyLightEnvMapSample.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSobol.h"
#include "Materials/MaterialExpressionSpeedTree.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionSRGBColorToWorkingColorSpace.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionSwitch.h"
#include "Materials/MaterialExpressionTangent.h"
#include "Materials/MaterialExpressionTangentOutput.h"
#include "Materials/MaterialExpressionTemporalSobol.h"
#include "Materials/MaterialExpressionTextureCollection.h"
#include "Materials/MaterialExpressionTextureCollectionParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectFromCollection.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTruncate.h"
#include "Materials/MaterialExpressionTruncateLWC.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionViewProperty.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Materials/MaterialExpressionVirtualTextureFeatureSwitch.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialInput.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "Materials/MaterialExpressionWhileLoop.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialShared.h"
#include "Misc/MemStackUtility.h"
#include "RenderUtils.h"
#include "ColorManagement/ColorSpace.h"

#include "Materials/MaterialIREmitter.h"

namespace IR = UE::MIR;

/* Constants */

void UMaterialExpression::Build(IR::FEmitter& Emitter)
{
	Emitter.Error(TEXT("Unsupported material expression."));
}

void UMaterialExpressionConstant::Build(IR::FEmitter& Emitter)
{
	IR::FValue* Value = Emitter.EmitConstantFloat1(R);
	Emitter.Put(GetOutput(0), Value);
}

void UMaterialExpressionConstant2Vector::Build(IR::FEmitter& Emitter)
{
	IR::FValue* Value = Emitter.EmitConstantFloat2({ R, G });
	Emitter.Put(GetOutput(0), Value);
}

void UMaterialExpressionConstant3Vector::Build(IR::FEmitter& Emitter)
{
	IR::FValue* Value = Emitter.EmitConstantFloat3({ Constant.R, Constant.G, Constant.B });
	Emitter.Put(GetOutput(0), Value);
}

void UMaterialExpressionConstant4Vector::Build(IR::FEmitter& Emitter)
{
	IR::FValue* Value = Emitter.EmitConstantFloat4(Constant);
	Emitter.Put(GetOutput(0), Value);
}

/* Mathematical Operations */

static void BuildBinaryArithmeticOperator(
	IR::FEmitter& Emitter,
	IR::EBinaryOperator Op,
	FExpressionInput* LhsInput,
	float LhsConst,
	FExpressionInput* RhsInput,
	float RhsConst,
	FExpressionOutput* Output)
{
	// Default inputs to their relative constants if disconnected, then get each input after checking it has arithmetic type.
	IR::FValue* LhsValue = Emitter.DefaultTo(LhsInput, LhsConst).TryGetArithmetic(LhsInput);
	IR::FValue* RhsValue = Emitter.DefaultTo(RhsInput, RhsConst).TryGetArithmetic(RhsInput);

	if (Emitter.IsInvalid())
	{
		return;
	}

	// Determine operation input/output type by looking at the first connected input and picking float1 otherwise.
	IR::FTypePtr ResultType = LhsValue ? LhsValue->Type
		: RhsValue ? RhsValue->Type
		: IR::FArithmeticType::GetScalar(IR::SK_Float);

	// Convert operand values to determined result type. 
	LhsValue = Emitter.TryEmitConstruct(ResultType, LhsValue);
	RhsValue = Emitter.TryEmitConstruct(ResultType, RhsValue);

	if (Emitter.IsInvalid())
	{
		return;
	}

	// Finally emit the binary operator.
	IR::FValue* Value = Emitter.EmitBinaryOperator(Op, LhsValue, RhsValue);

	// And flow it out of the expression's only output.
	Emitter.Put(Output, Value);
}

void UMaterialExpressionAdd::Build(IR::FEmitter& Emitter)
{ 
	BuildBinaryArithmeticOperator(Emitter, IR::BO_Add, &A, ConstA, &B, ConstB, GetOutput(0));
}

void UMaterialExpressionSubtract::Build(IR::FEmitter& Emitter)
{ 
	BuildBinaryArithmeticOperator(Emitter, IR::BO_Subtract, &A, ConstA, &B, ConstB, GetOutput(0));
}

void UMaterialExpressionMultiply::Build(IR::FEmitter& Emitter)
{ 
	BuildBinaryArithmeticOperator(Emitter, IR::BO_Multiply, &A, ConstA, &B, ConstB, GetOutput(0));
}

void UMaterialExpressionDivide::Build(IR::FEmitter& Emitter)
{ 
	BuildBinaryArithmeticOperator(Emitter, IR::BO_Divide, &A, ConstA, &B, ConstB, GetOutput(0));
}

void UMaterialExpressionIf::Build(IR::FEmitter& Emitter)
{
	// Create default values flowing into disconnected inputs
	Emitter.DefaultToFloatZero(&A);
	Emitter.DefaultTo(&B, ConstB);
	Emitter.DefaultToFloatZero(&AGreaterThanB);
	Emitter.DefaultToFloatZero(&AEqualsB);
	Emitter.DefaultToFloatZero(&ALessThanB);

	// Get input values and check their types are what we expect.
	IR::FValue* AValue = Emitter.TryGetScalar(&A);
	IR::FValue* BValue = Emitter.TryGetScalar(&B);
	IR::FValue* AGreaterThanBValue = Emitter.TryGetArithmetic(&AGreaterThanB);
	IR::FValue* AEqualsBValue = Emitter.TryGetArithmetic(&AEqualsB);
	IR::FValue* ALessThanBValue = Emitter.TryGetArithmetic(&ALessThanB);

	if (Emitter.IsInvalid())
	{
		return;
	}

	// Get the arithmetic common type between the conditional arguments (e.g. if inputs are int and float, it will return float).
	IR::FArithmeticTypePtr ConditionArgsType = Emitter.TryGetCommonArithmeticType(AValue->Type->AsArithmetic(), BValue->Type->AsArithmetic());

	if (Emitter.IsInvalid())
	{
		return;
	}

	// Convert both conditional argument values to the common type. This is expected to work as the common type could be found.
	AValue = Emitter.TryEmitConstruct(ConditionArgsType, AValue);
	BValue = Emitter.TryEmitConstruct(ConditionArgsType, BValue);

	// Now determine the output type by taking the common arithmetic type between result values.
	IR::FArithmeticTypePtr OutputType = Emitter.TryGetCommonArithmeticType(AGreaterThanBValue->Type->AsArithmetic(), AEqualsBValue->Type->AsArithmetic());
	OutputType = Emitter.TryGetCommonArithmeticType(OutputType, ALessThanBValue->Type->AsArithmetic());

	if (Emitter.IsInvalid())
	{
		return;
	}

	// Convert result values to the common result type.
	AGreaterThanBValue = Emitter.TryEmitConstruct(OutputType, AGreaterThanBValue);
	AEqualsBValue = Emitter.TryEmitConstruct(OutputType, AEqualsBValue);
	ALessThanBValue = Emitter.TryEmitConstruct(OutputType, ALessThanBValue);

	if (Emitter.IsInvalid())
	{
		return;
	}

	// Emit the comparison expressions.
	IR::FValue* ALessThanBConditionValue = Emitter.EmitBinaryOperator(IR::BO_Lower, AValue, BValue);
	IR::FValue* AEqualsBConditionValue = Emitter.EmitBinaryOperator(IR::BO_Equals, AValue, BValue);

	// And finally emit the full conditional expression.
	IR::FValue* OutputValue = Emitter.EmitBranch(AEqualsBConditionValue, AEqualsBValue, AGreaterThanBValue);
	OutputValue = Emitter.EmitBranch(ALessThanBConditionValue, ALessThanBValue, OutputValue);

	Emitter.Put(GetOutput(0), OutputValue);
}

#endif // WITH_EDITOR
