// Copyright Epic Games, Inc. All Rights Reserved.

#include "SettingsJsonImporter.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "HarmonixDsp/AudioUtility.h"
#include "HarmonixDsp/PitchShifterName.h"
#include "HarmonixDsp/FusionSampler/FusionPatch.h"
#include "HarmonixDsp/FusionSampler/Settings/FusionPatchSettings.h"
#include "HarmonixDsp/StretcherAndPitchShifterFactoryConfig.h"

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> SettingsJson, FKeyzoneSettings& Settings)
{
	FKeyzoneSettings Defaults;
	TryGetNumberField(SettingsJson, "root_note", Settings.RootNote, Defaults.RootNote);
	TryGetNumberField(SettingsJson, "min_note", Settings.MinNote, Defaults.MinNote);
	TryGetNumberField(SettingsJson, "max_note", Settings.MaxNote, Defaults.MaxNote);
	TryGetBoolField(SettingsJson, "unpitched", Settings.bUnpitched, Defaults.bUnpitched);
	TryGetBoolField(SettingsJson, "velocity_to_volume", Settings.bVelocityToGain, Defaults.bVelocityToGain);

	TryParseJson(SettingsJson, Settings.TimeStretchConfig);

	const TSharedPtr<FJsonObject>* TimeStretchJson = nullptr;
	if (SettingsJson->TryGetObjectField(TEXT("timestretch_settings"), TimeStretchJson))
	{
		TryParseJson(*TimeStretchJson, Settings.TimeStretchConfig);
	}

	TryGetNumberField(SettingsJson, "random_weight", Settings.RandomWeight, Defaults.RandomWeight);
	TryGetNumberField(SettingsJson, "min_velocity", Settings.MinVelocity, Defaults.MinVelocity);
	TryGetNumberField(SettingsJson, "max_velocity", Settings.MaxVelocity, Defaults.MaxVelocity);
	float VolumeDb;
	TryGetNumberField(SettingsJson, "volume", VolumeDb, HarmonixDsp::kDbSilence);
	Settings.SetVolumeDb(VolumeDb);

	const TSharedPtr<FJsonObject>* PanJson = nullptr;
	if (SettingsJson->TryGetObjectField(TEXT("pan"), PanJson))
	{
		TryParseJson(*PanJson, Settings.Pan);
	}

	TryGetNumberField(SettingsJson, "fine_tune", Settings.FineTuneCents, Defaults.FineTuneCents);
	Settings.SetFineTuneCents(Settings.FineTuneCents);

	TryGetBoolField(SettingsJson, "is_note_off_zone", Settings.bIsNoteOffZone, Defaults.bIsNoteOffZone);
	TryGetNumberField(SettingsJson, "priority", Settings.Priority, Defaults.Priority);

	TryGetStringField(SettingsJson, "sample_path", Settings.SamplePath);

	TryGetNumberField(SettingsJson, "start_offset_frame", Settings.SampleStartOffset, Defaults.SampleStartOffset);
	TryGetNumberField(SettingsJson, "end_offset_frame", Settings.SampleEndOffset, Defaults.SampleEndOffset);

	TryGetBoolField(SettingsJson, "singleton", Settings.UseSingletonVoicePool);

	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FTimeStretchConfig& TimeStretchConfig)
{
	FTimeStretchConfig Defaults;
	bool MaintainFormant = false;
	TryGetBoolField(JsonObj, "maintain_time", TimeStretchConfig.bMaintainTime, Defaults.bMaintainTime);
	TryGetBoolField(JsonObj, "maintain_formant", MaintainFormant);

	const UStretcherAndPitchShifterFactoryConfig* FactoryConfig = GetDefault<UStretcherAndPitchShifterFactoryConfig>();
	check(FactoryConfig);

	FName AlgorithmName = FactoryConfig->DefaultFactory.Name;
	TryGetNameField(JsonObj, "algorithm", AlgorithmName, AlgorithmName);

	if (AlgorithmName == "default")
	{
		TimeStretchConfig.PitchShifter = FactoryConfig->DefaultFactory;
	}
	else if (const FPitchShifterNameRedirect* Redirect = FactoryConfig->FindFactoryNameRedirect(AlgorithmName))
	{
		TimeStretchConfig.PitchShifter = Redirect->NewName;
	}
	else
	{
		TimeStretchConfig.PitchShifter = AlgorithmName;
	}

	TryGetBoolField(JsonObj, "sync_tempo", TimeStretchConfig.bSyncTempo, Defaults.bSyncTempo);
	TryGetNumberField(JsonObj, "orig_tempo", TimeStretchConfig.OriginalTempo, Defaults.OriginalTempo);

	int16 EnvelopeOrder = 0;
	JsonObj->TryGetNumberField(TEXT("envelope_order"), EnvelopeOrder);

	TimeStretchConfig.PitchShifterOptions.Add("EnvelopeOrder", EnvelopeOrder);
	TimeStretchConfig.PitchShifterOptions.Add("MaintainFormant", MaintainFormant);

	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FPannerDetails& OutPanDetails)
{
	FPannerDetails Defaults;
	TryGetEnumField(JsonObj, "mode", OutPanDetails.Mode, Defaults.Mode);
	switch (OutPanDetails.Mode)
	{
	case EPannerMode::LegacyStereo:
	case EPannerMode::Stereo:
		TryGetNumberField(JsonObj, "position", OutPanDetails.Detail.Pan, Defaults.Detail.Pan);
		OutPanDetails.Detail.EdgeProximity = 0.0f;
		return true;
	case EPannerMode::Surround:
	case EPannerMode::PolarSurround:
		TryGetNumberField(JsonObj, "position", OutPanDetails.Detail.Pan, Defaults.Detail.Pan);
		TryGetNumberField(JsonObj, "edge_proximity", OutPanDetails.Detail.EdgeProximity, Defaults.Detail.EdgeProximity);
		return true;
	
	case EPannerMode::DirectAssignment:
		TryGetEnumField(JsonObj, "position", OutPanDetails.Detail.ChannelAssignment, ESpeakerChannelAssignment::LeftFront);
		return true;
	}
	return false;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FFusionPatchSettings& Preset)
{
	FFusionPatchSettings Defaults;
	TryGetNumberField(JsonObj, "volume", Preset.VolumeDb, HarmonixDsp::kDbSilence);
	
	TryParseJson(JsonObj, Preset.PannerDetails);

	TryGetNumberField(JsonObj, "start_point", Preset.StartPointOffsetMs, Defaults.StartPointOffsetMs);
	TryGetNumberField(JsonObj, "fine_tune", Preset.FineTuneCents, Defaults.FineTuneCents);
	TryGetNumberField(JsonObj, "voice_limit", Preset.MaxVoices, 8);
	TryGetEnumField(JsonObj, "layer_select_mode", Preset.KeyzoneSelectMode, EKeyzoneSelectMode::Layers);

	// apply defaults just in case
	Preset.DownPitchBendCents = -700.0f;
	Preset.UpPitchBendCents = 700.0f;
	TSharedPtr<FJsonObject> PitchBendObj;
	if (TryGetObjectField(JsonObj, "pitch_bend", PitchBendObj))
	{
		const TArray<TSharedPtr<FJsonValue>>* RangeValues;
		if (PitchBendObj->TryGetArrayField(TEXT("range"), RangeValues))
		{
			if (ensure(RangeValues->Num() == 2))
			{
				TryGetNumber((*RangeValues)[0], Preset.DownPitchBendCents, Defaults.DownPitchBendCents);
				TryGetNumber((*RangeValues)[1], Preset.UpPitchBendCents, Defaults.UpPitchBendCents);
			}
		}
	}

	{
	int AdsrIdx = -1;
	for (TSharedPtr<FJsonValue> ArrayValue : IterField(JsonObj, "adsrs"))
	{
		++AdsrIdx;
		TSharedPtr<FJsonObject> AdsrObj;
		if (!TryGetObject(ArrayValue, AdsrObj))
			continue;

		TSharedPtr<FJsonObject> AdsrValue;
		if (!TryGetObjectField(AdsrObj, "adsr", AdsrValue))
			continue;

		FAdsrSettings& AdsrSettings = AdsrIdx == 0 ? Preset.Adsrs.Volume : Preset.Adsrs.Assignable;
		if (!TryParseJson(AdsrValue, AdsrSettings))
			continue;

		// old defaults
		if (AdsrIdx == 0 && !AdsrValue->HasField(TEXT("target")))
		{
			AdsrSettings.Target = EAdsrTarget::Volume;
			AdsrSettings.Depth = 1.0f;
			AdsrSettings.IsEnabled = true;
		}
	}
	}

	{
	int LfoIdx = -1;
	for (TSharedPtr<FJsonValue> ArrayValue : IterField(JsonObj, "lfos"))
	{
		++LfoIdx;

		if (!ensure(LfoIdx < Preset.Lfos.Num()))
			break;

		TSharedPtr<FJsonObject> LfoObj;
		if (!TryGetObject(ArrayValue, LfoObj))
			continue;

		TSharedPtr<FJsonObject> LfoValue;
		if (!TryGetObjectField(LfoObj, "lfo", LfoValue))
			continue;

		FLfoSettings& LfoSettings = Preset.Lfos[LfoIdx];

		if (!TryParseJson(LfoValue, LfoSettings))
			continue;

		if (!LfoValue->HasField(TEXT("target")))
		{
			// old defaults
			if (LfoIdx == 0)
				LfoSettings.Target = ELfoTarget::Pan;
			else if (LfoIdx == 1)
				LfoSettings.Target = ELfoTarget::Pitch;
		}
	}
	}

	TSharedPtr<FJsonObject> FilterObj;
	if (TryGetObjectField(JsonObj, "filter", FilterObj))
	{
		TryParseJson(FilterObj, Preset.Filter);
	}
	else
	{
		Preset.Filter.IsEnabled = false;
	}

	{
	int RandomizerIdx = -1;
	for (TSharedPtr<FJsonValue> ArrayValue : IterField(JsonObj, "randomizers"))
	{
		++RandomizerIdx;
		if (!ensure(RandomizerIdx < Preset.Randomizers.Num()))
			break;

		TSharedPtr<FJsonObject> RandomizerObj;
		if (!TryGetObject(ArrayValue, RandomizerObj))
			continue;

		TSharedPtr<FJsonObject> RandomizerValue;
		if (!TryGetObjectField(RandomizerObj, "modulator", RandomizerValue))
			continue;

		TryParseJson(RandomizerValue, Preset.Randomizers[RandomizerIdx]);
	}
	}

	{
	int ModIdx = -1;
	for (TSharedPtr<FJsonValue> ArrayValue : IterField(JsonObj, "velocity_mods"))
	{
		++ModIdx;
		if (!ensure(ModIdx < Preset.VelocityModulators.Num()))
			break;

		TSharedPtr<FJsonObject> VelocityModObj;
		if (!TryGetObject(ArrayValue, VelocityModObj))
			continue;

		TSharedPtr<FJsonObject> VelocityModValue;
		if (!TryGetObjectField(VelocityModObj, "modulator", VelocityModValue))
			continue;

		TryParseJson(VelocityModValue, Preset.VelocityModulators[ModIdx]);
	}
	}

	{
	TSharedPtr<FJsonObject> DelayObj;
	if (TryGetObjectField(JsonObj, "delay_effect", DelayObj))
	{
		TryParseJson(DelayObj, Preset.Delay);
	}
	else
	{
		Preset.Delay.IsEnabled = false;
	}
	}

	{
	TSharedPtr<FJsonObject> BitcrushObj;
	if (TryGetObjectField(JsonObj, "bitcrush_effect", BitcrushObj))
	{
		TryParseJson(BitcrushObj, Preset.BitCrusher);
	}
	else
	{
		Preset.BitCrusher.IsEnabled = false;
	}
	}

	{
	TSharedPtr<FJsonObject> VocoderObj;
	if (TryGetObjectField(JsonObj, "vocoder_effect", VocoderObj))
	{
		TryParseJson(VocoderObj, Preset.Vocoder);
	}
	else
	{
		Preset.Vocoder.IsEnabled = false;
	}
	}

	{
	TSharedPtr<FJsonObject> DistortionObj;
	if (TryGetObjectField(JsonObj, "distortion_effect", DistortionObj))
	{
		TryParseJson(DistortionObj, Preset.Distortion);
	}
	else
	{
		Preset.Distortion.IsEnabled = false;
	}
	}

	{
	TSharedPtr<FJsonObject> PortamentoObj;
	if (TryGetObjectField(JsonObj, "portamento", PortamentoObj))
	{
		TryParseJson(PortamentoObj, Preset.Portamento);
	}
	else
	{
		Preset.Portamento.IsEnabled = false;
		Preset.Portamento.Mode = EPortamentoMode::Legato;
		Preset.Portamento.Seconds = 0.0f;
	}
	}

	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FAdsrSettings& AdsrSettings)
{
	// simplify the conversion process :)
	static TFunction<float(float)> MsToSec = [](float Ms) { return Ms * 0.001f; };
	TryGetEnumField(JsonObj, "target", AdsrSettings.Target, EAdsrTarget::None);
	TryGetNumberField(JsonObj, "depth", AdsrSettings.Depth, 0.0f);
	TryGetBoolField(JsonObj, "enabled", AdsrSettings.IsEnabled, false);
	TryGetNumberField(JsonObj, "attack", AdsrSettings.AttackTime, MsToSec, 1.0f);
	TryGetNumberField(JsonObj, "attack_curve", AdsrSettings.AttackCurve, 0.0f);
	TryGetNumberField(JsonObj, "decay", AdsrSettings.DecayTime, MsToSec, 1.0f);
	TryGetNumberField(JsonObj, "decay_curve", AdsrSettings.DecayCurve, 0.0f);
	TryGetNumberField(JsonObj, "sustain", AdsrSettings.SustainLevel, 1.0f);
	TryGetNumberField(JsonObj, "release", AdsrSettings.ReleaseTime, MsToSec, 1.0f);
	TryGetNumberField(JsonObj, "release_curve", AdsrSettings.ReleaseCurve, 0.0f);

	AdsrSettings.Calculate();

	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FLfoSettings& LfoSettings)
{
	TryGetEnumField(JsonObj, "target", LfoSettings.Target, ELfoTarget::None);
	TryGetNumberField(JsonObj, "frequency", LfoSettings.Freq);
	TryGetNumberField(JsonObj, "depth", LfoSettings.Depth);
	TryGetBoolField(JsonObj, "enabled", LfoSettings.IsEnabled);
	TryGetEnumField(JsonObj, "shape", LfoSettings.Shape, EWaveShape::None);
	TryGetBoolField(JsonObj, "retrigger", LfoSettings.ShouldRetrigger, false);
	TryGetNumberField(JsonObj, "initial_phase", LfoSettings.InitialPhase, 0.0f);
	TryGetBoolField(JsonObj, "beat_sync", LfoSettings.BeatSync, false);
	TryGetNumberField(JsonObj, "tempo", LfoSettings.TempoBPM, 120.0f);

	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FBiquadFilterSettings& Filter)
{
	TryGetBoolField(JsonObj, "enabled", Filter.IsEnabled);
	TryGetEnumField(JsonObj, "type", Filter.Type, EBiquadFilterType::None);
	TryGetNumberField(JsonObj, "frequency", Filter.Freq);
	TryGetNumberField(JsonObj, "q", Filter.Q);
	TryGetNumberField(JsonObj, "gain_db", Filter.DesignedDBGain, 0.0f);

	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FModulatorSettings& ModulatorSettings)
{
	TryGetEnumField(JsonObj, "target", ModulatorSettings.Target, EModulatorTarget::None);
	TryGetNumberField(JsonObj, "range", ModulatorSettings.Range, 0.0f);
	TryGetNumberField(JsonObj, "depth", ModulatorSettings.Depth, 0.0f);
	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FDelaySettings& DelaySettings)
{
	TryGetBoolField(JsonObj, "enabled", DelaySettings.IsEnabled, false);

	{
		bool OldBeatSyncSetting;
		TryGetBoolField(JsonObj, "beat_sync", OldBeatSyncSetting, false);
		TryGetEnumField(JsonObj, "time_sync", DelaySettings.TimeSyncOption, OldBeatSyncSetting ? ETimeSyncOption::TempoSync : ETimeSyncOption::None);
	}

	TryGetNumberField(JsonObj, "time", DelaySettings.TimeSeconds, 0.5f);
	TryGetNumberField(JsonObj, "dry_gain", DelaySettings.DryGain, 1.0f);
	TryGetNumberField(JsonObj, "wet_gain", DelaySettings.WetGain, 0.5f);
	TryGetNumberField(JsonObj, "feedback", DelaySettings.FeedbackGain, 0.25f);
	TryGetBoolField(JsonObj, "delay_eq_enabled", DelaySettings.EQEnabled, false);
	TryGetEnumField(JsonObj, "delay_eq_type", DelaySettings.EQType, EDelayFilterType::LowPass);
	TryGetNumberField(JsonObj, "delay_eq_frequency", DelaySettings.EQFreq, 600.0f);
	TryGetNumberField(JsonObj, "delay_eq_q", DelaySettings.EQQ, 1.0f);
	TryGetBoolField(JsonObj, "delay_lfo_enabled", DelaySettings.LfoEnabled, false);

	{
		bool OldBeatSyncSetting;
		TryGetBoolField(JsonObj, "delay_lfo_beatsync", OldBeatSyncSetting, false);
		TryGetEnumField(JsonObj, "delay_lfo_time_sync", DelaySettings.LfoTimeSyncOption, OldBeatSyncSetting ? ETimeSyncOption::TempoSync : ETimeSyncOption::None);
	}

	TryGetNumberField(JsonObj, "delay_lfo_rate", DelaySettings.LfoRate, 0.0f);
	TryGetNumberField(JsonObj, "delay_lfo_depth", DelaySettings.LfoDepth, 0.0f);
	TryGetEnumField(JsonObj, "delay_stereo_type", DelaySettings.StereoType, EDelayStereoType::Default);
	TryGetNumberField(JsonObj, "delay_stereo_left", DelaySettings.PanLeft, 0.0f);
	TryGetNumberField(JsonObj, "delay_stereo_right", DelaySettings.PanRight, 0.0f);
	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FBitCrusherSettings& BitCrusherSettings)
{
	static TFunction<float(float)> PctToGain = [](float Pct) { return Pct / 100.0f; };

	TryGetBoolField(JsonObj, "enabled", BitCrusherSettings.IsEnabled, false);
	TryGetNumberField(JsonObj, "wet_percent", BitCrusherSettings.WetGain, PctToGain, 1.0f);
	TryGetNumberField(JsonObj, "crush_level", BitCrusherSettings.Crush, (uint16)4);
	TryGetNumberField(JsonObj, "sample_hold", BitCrusherSettings.SampleHoldFactor, (uint16)5);
	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FVocoderSettings& VocoderSettings)
{
	TryGetBoolField(JsonObj, "enabled", VocoderSettings.IsEnabled, false);
	TryGetNumberField(JsonObj, "modulator_index", VocoderSettings.ModulatorIndex, -1);
	if (VocoderSettings.ModulatorIndex == -1)
	{
		// index name intentionally masked
		FString DeprecatedIndex = "\x73\x6c\x61\x76\x65\x5f\x69\x6e\x64\x65\x78";
		TryGetNumberField(JsonObj, DeprecatedIndex, VocoderSettings.ModulatorIndex, 0);
	}

	TryGetEnumField(JsonObj, "band_config_index", VocoderSettings.BandConfig, EVocoderBandConfig::k64);
	TryGetNumberField(JsonObj, "carrier_gain", VocoderSettings.CarrierGain, 1.0f);
	TryGetNumberField(JsonObj, "carrier_thin", VocoderSettings.CarrierThin, 1.0f);
	TryGetNumberField(JsonObj, "modulator_gain", VocoderSettings.ModulatorGain, 1.0f);
	TryGetNumberField(JsonObj, "modultar_thin", VocoderSettings.ModulatorThin, 1.0f);
	TryGetNumberField(JsonObj, "attack", VocoderSettings.Attack, 0.5f);
	TryGetNumberField(JsonObj, "release", VocoderSettings.Release, 0.5f);
	TryGetNumberField(JsonObj, "high_emphasis", VocoderSettings.HighEmphasis, 0.0f);
	TryGetNumberField(JsonObj, "output_gain_db", VocoderSettings.OutputGain, 1.0f);
	TryGetBoolField(JsonObj, "solo", VocoderSettings.Soloing, false);

	{
	const TArray<TSharedPtr<FJsonValue>>* BandArray;
	if (JsonObj->TryGetArrayField(TEXT("band"), BandArray))
	{
		VocoderSettings.Bands.SetNum(BandArray->Num());

		for (TSharedPtr<FJsonValue> BandValue : *BandArray)
		{
			TSharedPtr<FJsonObject> BandObj;
			if (!TryGetObject(BandValue, BandObj))
				continue;

			VocoderSettings.Bands.SetNum(VocoderSettings.Bands.Num() + 1);

			TryGetNumberField(BandObj, "gain", VocoderSettings.Bands.Last().Gain, 1.0f);
			TryGetBoolField(BandObj, "solo", VocoderSettings.Bands.Last().Solo, false);
		}
	}
	}
	return true;
}

bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FDistortionSettingsV1& DistortionSettings)
{
	TryGetBoolField(JsonObj, "enabled", DistortionSettings.IsEnabled, false);
	TryGetNumberField(JsonObj, "input_gain", DistortionSettings.InputGainDb, 0.0f);
	TryGetNumberField(JsonObj, "output_gain", DistortionSettings.OutputGainDb, 0.0f);
	TryGetNumberField(JsonObj, "dc_adjust", DistortionSettings.DCAdjust, 0.0f);
	TryGetEnumField(JsonObj, "type", DistortionSettings.Type, EDistortionTypeV1::Clean);
	TryGetBoolField(JsonObj, "oversample", DistortionSettings.Oversample, false);

	{
	int FilterIdx = -1;
	for (TSharedPtr<FJsonValue> ArrayValue : IterField(JsonObj, "filter_set"))
	{
		++FilterIdx;

		if (!ensureAlways(FilterIdx < FDistortionSettingsV1::kNumFilters))
			break;

		TSharedPtr<FJsonObject> FilterObj;
		if (!TryGetObject(ArrayValue, FilterObj))
			continue;

		TSharedPtr<FJsonObject> FilterValue;
		if (!TryGetObjectField(FilterObj, "filter", FilterValue))
			continue;

		TryParseJson(FilterValue, DistortionSettings.Filters[FilterIdx]);
	}
	}
	return true;
}


bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FDistortionFilterSettings& DistortionFilterSettings)
{
	TryParseJson(JsonObj, DistortionFilterSettings.Filter);
	TryGetBoolField(JsonObj, "filter_pre", DistortionFilterSettings.FilterPreClip);
	TryGetNumberField(JsonObj, "num_passes", DistortionFilterSettings.NumPasses);

	return true;
}


bool FSettingsJsonImporter::TryParseJson(TSharedPtr<FJsonObject> JsonObj, FPortamentoSettings& PortamentoSettings)
{
	TryGetBoolField(JsonObj, "enabled", PortamentoSettings.IsEnabled, false);
	TryGetEnumField(JsonObj, "mode", PortamentoSettings.Mode, EPortamentoMode::Legato);
	TryGetNumberField(JsonObj, "time", PortamentoSettings.Seconds, 0.0f);
	return true;
}
