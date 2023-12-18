// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioSpectrumAnalyzer.h"

#include "DSP/EnvelopeFollower.h"

namespace AudioWidgets
{
	FAudioSpectrumAnalyzer::FAudioSpectrumAnalyzer(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus)
		: Widget(SNew(SAudioSpectrumPlot)
			.Clipping(EWidgetClipping::ClipToBounds)
			.DisplayFrequencyAxisLabels(false)
			.DisplaySoundLevelAxisLabels(false)
			.OnGetAudioSpectrumData_Raw(this, &FAudioSpectrumAnalyzer::GetAudioSpectrumData))
	{
		Init(InNumChannels, InAudioDeviceId, InExternalAudioBus);
	}

	FAudioSpectrumAnalyzer::~FAudioSpectrumAnalyzer()
	{
		Teardown();

		Widget->UnbindOnGetAudioSpectrumData();
	}

	UAudioBus* FAudioSpectrumAnalyzer::GetAudioBus() const
	{
		return AudioBus.Get();
	}

	TSharedRef<SWidget> FAudioSpectrumAnalyzer::GetWidget() const
	{
		return Widget->AsShared();
	}

	void FAudioSpectrumAnalyzer::Init(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus)
	{
		check(InNumChannels > 0);

		Teardown();

		Settings = TStrongObjectPtr(NewObject<UConstantQSettings>());
		Settings->SpectrumType = EAudioSpectrumType::PowerSpectrum;
		Settings->NumBandsPerOctave = 6.0f;
		Settings->NumBands = 61;
		Settings->StartingFrequencyHz = 20000.0f * FMath::Pow(0.5f, (Settings->NumBands - 1) / Settings->NumBandsPerOctave);
		Settings->FFTSize = EConstantQFFTSizeEnum::XXLarge;
		Settings->bDownmixToMono = true;
		Settings->BandWidthStretch = 2.0f;

		Analyzer = TStrongObjectPtr(NewObject<UConstantQAnalyzer>());
		Analyzer->Settings = Settings.Get();

		bUseExternalAudioBus = InExternalAudioBus != nullptr;

		AudioBus = bUseExternalAudioBus ? TStrongObjectPtr(InExternalAudioBus.Get()) : TStrongObjectPtr(NewObject<UAudioBus>());
		AudioBus->AudioBusChannels = EAudioBusChannels(InNumChannels - 1);

		ResultsDelegateHandle = Analyzer->OnConstantQResultsNative.AddRaw(this, &FAudioSpectrumAnalyzer::OnConstantQResults);

		Analyzer->StartAnalyzing(InAudioDeviceId, AudioBus.Get());
	}

	void FAudioSpectrumAnalyzer::OnConstantQResults(UConstantQAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FConstantQResults>& InSpectrumResultsArray)
	{
		if (InSpectrumAnalyzer == Analyzer.Get())
		{
			for (const FConstantQResults& SpectrumResults : InSpectrumResultsArray)
			{
				if (PrevTimeStamp.IsSet() && SpectrumResults.TimeSeconds > PrevTimeStamp.GetValue())
				{
					// Calculate AR smoother coefficients:
					const float DeltaT = (SpectrumResults.TimeSeconds - PrevTimeStamp.GetValue());
					Audio::FAttackRelease AttackRelease(1.0f / DeltaT, AttackTimeMsec, ReleaseTimeMsec, true);

					// Apply AR smoothing for each frequency:
					check(SpectrumResults.SpectrumValues.Num() == ARSmoothedSquaredMagnitudes.Num());
					for (int Index = 0; Index < SpectrumResults.SpectrumValues.Num(); Index++)
					{
						const float OldValue = ARSmoothedSquaredMagnitudes[Index];
						const float NewValue = SpectrumResults.SpectrumValues[Index];
						const float ARSmootherCoefficient = (NewValue >= OldValue) ? AttackRelease.GetAttackTimeSamples() : AttackRelease.GetReleaseTimeSamples();
						const float SmoothedValue = FMath::Lerp(NewValue, OldValue, ARSmootherCoefficient);
						ARSmoothedSquaredMagnitudes[Index] = Audio::UnderflowClamp(SmoothedValue);
					}
				}
				else
				{
					// Init center frequencies:
					CenterFrequencies.SetNumUninitialized(Analyzer->GetNumCenterFrequencies());
					Analyzer->GetCenterFrequencies(CenterFrequencies);

					// Init spectrum data:
					ARSmoothedSquaredMagnitudes = SpectrumResults.SpectrumValues;
				}

				PrevTimeStamp = SpectrumResults.TimeSeconds;
			}
		}
	}

	void FAudioSpectrumAnalyzer::Teardown()
	{
		if (Analyzer.IsValid() && Analyzer->IsValidLowLevel())
		{
			Analyzer->StopAnalyzing();
			if (ResultsDelegateHandle.IsValid())
			{
				Analyzer->OnConstantQResultsNative.Remove(ResultsDelegateHandle);
			}
			
			Analyzer.Reset();
		}

		ResultsDelegateHandle.Reset();
		PrevTimeStamp.Reset();
		CenterFrequencies.Empty();
		ARSmoothedSquaredMagnitudes.Empty();

		AudioBus.Reset();
		Settings.Reset();

		bUseExternalAudioBus = false;
	}

	FAudioPowerSpectrumData FAudioSpectrumAnalyzer::GetAudioSpectrumData() const
	{
		check(CenterFrequencies.Num() == ARSmoothedSquaredMagnitudes.Num());
		return FAudioPowerSpectrumData{ CenterFrequencies, ARSmoothedSquaredMagnitudes };
	}
} // namespace AudioWidgets
