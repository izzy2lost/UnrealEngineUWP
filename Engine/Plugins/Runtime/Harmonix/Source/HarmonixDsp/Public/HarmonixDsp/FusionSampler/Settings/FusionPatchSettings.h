// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/Containers/EnumArray.h"
#include "HarmonixDsp/PannerDetails.h"
#include "HarmonixDsp/Effects/Settings/BiquadFilterSettings.h"
#include "HarmonixDsp/Effects/Settings/BitCrusherSettings.h"
#include "HarmonixDsp/Effects/Settings/DelaySettings.h"
#include "HarmonixDsp/Effects/Settings/DistortionSettings.h"
#include "HarmonixDsp/Effects/Settings/VocoderSettings.h"
#include "HarmonixDsp/Modulators/Settings/AdsrSettings.h"
#include "HarmonixDsp/Modulators/Settings/LfoSettings.h"
#include "HarmonixDsp/Modulators/Settings/ModulatorSettings.h"
#include "HarmonixDsp/Modulators/ModulatorTarget.h"
#include "HarmonixDsp/FusionSampler/Settings/KeyzoneSettings.h"
#include "HarmonixDsp/FusionSampler/Settings/PortamentoSettings.h"


#include "FusionPatchSettings.generated.h"

UENUM()
enum class EKeyzoneSelectMode : uint8
{
	Layers UMETA(Json="layers"),
	Random UMETA(Json="random"),
	RandomWithRepetition UMETA(Json="random_with_repititon"),
	Cycle UMETA(Json="cycle"),
	Num UMETA(Hidden),
	Invalid UMETA(Hidden)
};

USTRUCT()
struct HARMONIXDSP_API FFusionPatchSettings
{
	GENERATED_BODY()

public:

	FFusionPatchSettings();
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings", Meta = (ClampMin = "-96", ClampMax = "12", UIMin = "-96", UIMax = "12"))
	float VolumeDb = 0.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FPannerDetails PannerDetails;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings", Meta = (ClampMin = "-2400", ClampMax = "-200", UIMin = "-2400", UIMax = "-200"))
	float DownPitchBendCents = -200.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings", Meta = (ClampMin = "200", ClampMax = "2400", UIMin = "200", UIMax = "2400"))
	float UpPitchBendCents = 200.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings", Meta = (ClampMin = "-100", ClampMax = "100", UIMin = "-100", UIMax = "100"))
	float FineTuneCents = 0.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings", DisplayName = "Start Point Offset (Ms)", Meta = (ClampMin = "0", ClampMax = "1000", UIMin = "0", UIMax = "1000"))
	float StartPointOffsetMs = 0.0f;
	
	UPROPERTY()
	int32 MaxVoices = 32;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	EKeyzoneSelectMode KeyzoneSelectMode = EKeyzoneSelectMode::Layers;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FPortamentoSettings Portamento;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FBiquadFilterSettings Filter;
	
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FDelaySettings Delay;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FDistortionSettingsV1 Distortion;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FBitCrusherSettings BitCrusher;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FVocoderSettings Vocoder;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FAdsrSettingsArray Adsrs;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FLfoSettingsArray Lfos;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FModulatorSettingsArray Randomizers;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FModulatorSettingsArray VelocityModulators;
};