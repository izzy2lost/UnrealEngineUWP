// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/FusionSampler/Settings/KeyzoneSettings.h"
#include "HarmonixDsp/FusionSampler/SingletonFusionVoicePool.h"
#include "HarmonixDsp/FusionSampler/FusionPatch.h"
#include "HarmonixDsp/AudioUtility.h"
#include "HarmonixDsp/AudioData.h"
#include "HarmonixDsp/AudioData/StreamingAudioData.h"

#include "Sound/SoundWave.h"

#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY(LogKeyzoneSettings);

void FKeyzoneSettings::InitProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FKeyzoneSettings_InitProxyData);

	UE_LOG(LogKeyzoneSettings, Verbose, TEXT("Initializing new KeyzoneSettings proxy"));
	
	// we should only be initializing the proxy data if the sound wave is valid
	if (!ensure(SoundWave))
	{
		return;
	}

	AudioSample = CreateStreamingAudioData(InitParams);

	check(AudioSample);

	// nulling out sound wave asset for this Keyzone
	// now that we are turning into ProxyData
	SoundWave = nullptr;

	if (UseSingletonVoicePool && !SingletonFusionVoicePool)
	{
		SingletonFusionVoicePool = MakeShared<FSingletonFusionVoicePool>(FSingletonFusionVoicePool::kMaxSingletonAliases, *this);
	}
	else if (SingletonFusionVoicePool)
	{
		SingletonFusionVoicePool.Reset();
	}
}

TSharedPtr<FStreamingAudioData, ESPMode::ThreadSafe> FKeyzoneSettings::CreateStreamingAudioData(const Audio::FProxyDataInitParams& InitParams) const
{
	TSharedPtr<FStreamingAudioData, ESPMode::ThreadSafe> StreamingAudioSample = StaticCastSharedPtr<FStreamingAudioData>(AudioSample);
	if (StreamingAudioSample.IsValid())
	{
		return StreamingAudioSample;
	}

	if (!ensure(SoundWave))
	{
		UE_LOG(LogKeyzoneSettings, Error, TEXT("SoundWave asset is null in keyzone settings when making proxy data!!"));
		return nullptr;
	}

	TSharedPtr<Audio::IProxyData, ESPMode::ThreadSafe> AudioProxy = SoundWave->CreateProxyData(InitParams);

	FSoundWaveProxyPtr WaveProxy = StaticCastSharedPtr<FSoundWaveProxy>(AudioProxy);
	if (!ensure(WaveProxy))
	{
		UE_LOG(LogKeyzoneSettings, Error, TEXT("Failed to make sound wave proxy data for SoundWave asset!!"));
		return nullptr;
	}

	UE_LOG(LogKeyzoneSettings, Verbose, TEXT("Creating new shared AudioData"));

	FStreamingAudioData::FSettings StreamingAudioSettings;
	StreamingAudioSample = FStreamingAudioData::GetShared<FStreamingAudioData>(WaveProxy.ToSharedRef(), StreamingAudioSettings);
	return StreamingAudioSample;
}

bool FKeyzoneSettings::ContainsNoteAndVelocity(uint8 InNote, uint8 InVelocity) const
{
	return (InNote >= MinNote && InNote <= MaxNote) 
		&& (InVelocity >= MinVelocity && InVelocity <= MaxVelocity);
}

void FKeyzoneSettings::SetVolumeDb(float Db)
{ 
	Db = HarmonixDsp::ClampDB(Db);
	Gain = HarmonixDsp::DBToLinear(Db);
}

float FKeyzoneSettings::GetVolumeDb() const
{
	return HarmonixDsp::LinearToDB(FMath::Clamp(Gain, 0.0f, 1.0f));
}

void FKeyzoneSettings::SetFineTuneCents(float InCents)
{
	FineTuneCents = InCents;
	FineTuneAdjustment = FMath::Pow(2.0f, FineTuneCents / 1200.0f);
}