// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/PostProcessVolume.h"
#include "AvaPostProcessVolume.generated.h"

/** 
 * Motion Design Post Process Volume is derived from Post Process Volume.
 * Its function is to provide a Post Process which can be used right away inside Motion Design.
 * This is done by customizing some of the default values.
 */
UCLASS(MinimalAPI, DisplayName = "Motion Design Post Process Volume")
class AAvaPostProcessVolume : public APostProcessVolume
{
	GENERATED_BODY()

public:
	AAvaPostProcessVolume()
	{
		BlendWeight = 1.f;
		bUnbound = true;

		Settings.bOverride_AutoExposureBias = 1;
		Settings.AutoExposureBias = 0.f;

		Settings.bOverride_AutoExposureMinBrightness = 1;
		Settings.AutoExposureMinBrightness = 1.f;

		Settings.bOverride_AutoExposureMaxBrightness = 1;
		Settings.AutoExposureMaxBrightness = 1.f;

		Settings.bOverride_ExpandGamut = 1;
		Settings.ExpandGamut = 0.f;

		Settings.bOverride_ToneCurveAmount = 1;
		Settings.ToneCurveAmount = 0.f;

		Settings.bOverride_BlueCorrection = 1;
		Settings.BlueCorrection = 0.f;

		Settings.bOverride_VignetteIntensity = 1;
		Settings.VignetteIntensity = 0.f;
	}
};
