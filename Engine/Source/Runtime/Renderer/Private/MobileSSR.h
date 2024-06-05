// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "SceneRendering.h"
#include "ScreenSpaceRayTracing.h"

BEGIN_SHADER_PARAMETER_STRUCT(FMobileScreenSpaceReflectionParams, )
SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneColor)
SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZB)
SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
SHADER_PARAMETER(FVector4f, HZBUvFactorAndInvFactor)
SHADER_PARAMETER(FVector4f, PrevScreenPositionScaleBias)
SHADER_PARAMETER(FVector4f, PrevSceneColorBilinearUVMinMax)
SHADER_PARAMETER(FVector4f, QualityAndExposureCorrection) // .x = SSRQuality, .y = PrevSceneColorPreExposureInv, .z = PrevSceneColorPreExposureCorrection, .w = View.FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness
END_SHADER_PARAMETER_STRUCT()

bool IsMobileSSREnabled(const FViewInfo& View);
bool ShouldRenderMobileSSR(const FViewInfo& View);

void SetupMobileSSRParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FMobileScreenSpaceReflectionParams& Params);
