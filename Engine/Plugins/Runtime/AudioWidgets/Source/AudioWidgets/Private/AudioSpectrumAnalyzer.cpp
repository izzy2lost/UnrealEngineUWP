// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioSpectrumAnalyzer.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "DSP/EnvelopeFollower.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FAudioSpectrumAnalyzer"

namespace AudioWidgets
{
	FAudioSpectrumAnalyzer::FAudioSpectrumAnalyzer(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus)
		: SpectrumAnalysisSettings(NewObject<USynesthesiaSpectrumAnalysisSettings>())
		, ConstantQSettings(NewObject<UConstantQSettings>())
		, Widget(SNew(SAudioSpectrumPlot)
			.Clipping(EWidgetClipping::ClipToBounds)
			.DisplayFrequencyAxisLabels(false)
			.DisplaySoundLevelAxisLabels(false)
			.OnGetAudioSpectrumData_Raw(this, &FAudioSpectrumAnalyzer::GetAudioSpectrumData))
	{
		SpectrumAnalysisSettings->SpectrumType = EAudioSpectrumType::PowerSpectrum;
		SpectrumAnalysisSettings->FFTSize = EFFTSize::Max;
		SpectrumAnalysisSettings->WindowType = EFFTWindowType::Blackman;
		SpectrumAnalysisSettings->bDownmixToMono = true;

		ConstantQSettings->SpectrumType = EAudioSpectrumType::PowerSpectrum;
		ConstantQSettings->NumBandsPerOctave = 6.0f;
		ConstantQSettings->NumBands = 61;
		ConstantQSettings->StartingFrequencyHz = 20000.0f * FMath::Pow(0.5f, (ConstantQSettings->NumBands - 1) / ConstantQSettings->NumBandsPerOctave);
		ConstantQSettings->FFTSize = EConstantQFFTSizeEnum::XXLarge;
		ConstantQSettings->WindowType = EFFTWindowType::Blackman;
		ConstantQSettings->bDownmixToMono = true;
		ConstantQSettings->BandWidthStretch = 2.0f;

		ContextMenuExtension = Widget->AddContextMenuExtension(EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateRaw(this, &FAudioSpectrumAnalyzer::ExtendSpectrumPlotContextMenu));

		Init(InNumChannels, InAudioDeviceId, InExternalAudioBus);
	}

	FAudioSpectrumAnalyzer::~FAudioSpectrumAnalyzer()
	{
		Teardown();

		Widget->UnbindOnGetAudioSpectrumData();

		if (ContextMenuExtension.IsValid())
		{
			Widget->RemoveContextMenuExtension(ContextMenuExtension.ToSharedRef());
		}
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

		AudioDeviceId = InAudioDeviceId;
		bUseExternalAudioBus = InExternalAudioBus != nullptr;
		AudioBus = bUseExternalAudioBus ? TStrongObjectPtr(InExternalAudioBus.Get()) : TStrongObjectPtr(NewObject<UAudioBus>());
		AudioBus->AudioBusChannels = EAudioBusChannels(InNumChannels - 1);

		CreateSynesthesiaSpectrumAnalyzer();
		CreateConstantQAnalyzer();

		StartAnalyzing();
	}

	void FAudioSpectrumAnalyzer::StartAnalyzing()
	{
		switch (AnalyzerType)
		{
		case EAudioSpectrumAnalyzerType::FFT:
			SpectrumAnalyzer->StartAnalyzing(AudioDeviceId, AudioBus.Get());
			break;
		case EAudioSpectrumAnalyzerType::CQT:
			ConstantQAnalyzer->StartAnalyzing(AudioDeviceId, AudioBus.Get());
			break;
		default:
			break;
		}
	}

	void FAudioSpectrumAnalyzer::StopAnalyzing()
	{
		switch (AnalyzerType)
		{
		case EAudioSpectrumAnalyzerType::FFT:
			SpectrumAnalyzer->StopAnalyzing();
			break;
		case EAudioSpectrumAnalyzerType::CQT:
			ConstantQAnalyzer->StopAnalyzing();
			break;
		default:
			break;
		}
	}

	void FAudioSpectrumAnalyzer::OnSpectrumResults(USynesthesiaSpectrumAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FSynesthesiaSpectrumResults>& InSpectrumResultsArray)
	{
		if (AnalyzerType == EAudioSpectrumAnalyzerType::FFT && InSpectrumAnalyzer == SpectrumAnalyzer.Get())
		{
			for (const FSynesthesiaSpectrumResults& SpectrumResults : InSpectrumResultsArray)
			{
				if (PrevTimeStamp.IsSet() && SpectrumResults.TimeSeconds > PrevTimeStamp.GetValue())
				{
					UpdateARSmoothing(SpectrumResults.TimeSeconds, SpectrumResults.SpectrumValues);
				}
				else
				{
					// Find samplerate:
					float SampleRate = 48000.0f;
					if (const FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
					{
						if (const FAudioDevice* AudioDevice = AudioDeviceManager->GetAudioDeviceRaw(AudioDeviceId))
						{
							SampleRate = AudioDevice->GetSampleRate();
						}
					}

					// Init center frequencies:
					CenterFrequencies.SetNumUninitialized(SpectrumAnalyzer->GetNumCenterFrequencies());
					SpectrumAnalyzer->GetCenterFrequencies(SampleRate, CenterFrequencies);

					// Init spectrum data:
					ARSmoothedSquaredMagnitudes = SpectrumResults.SpectrumValues;
				}

				PrevTimeStamp = SpectrumResults.TimeSeconds;
			}
		}
	}

	void FAudioSpectrumAnalyzer::OnConstantQResults(UConstantQAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FConstantQResults>& InSpectrumResultsArray)
	{
		if (AnalyzerType == EAudioSpectrumAnalyzerType::CQT && InSpectrumAnalyzer == ConstantQAnalyzer.Get())
		{
			for (const FConstantQResults& SpectrumResults : InSpectrumResultsArray)
			{
				if (PrevTimeStamp.IsSet() && SpectrumResults.TimeSeconds > PrevTimeStamp.GetValue())
				{
					UpdateARSmoothing(SpectrumResults.TimeSeconds, SpectrumResults.SpectrumValues);
				}
				else
				{
					// Init center frequencies:
					CenterFrequencies.SetNumUninitialized(ConstantQAnalyzer->GetNumCenterFrequencies());
					ConstantQAnalyzer->GetCenterFrequencies(CenterFrequencies);

					// Init spectrum data:
					ARSmoothedSquaredMagnitudes = SpectrumResults.SpectrumValues;
				}

				PrevTimeStamp = SpectrumResults.TimeSeconds;
			}
		}
	}

	void FAudioSpectrumAnalyzer::UpdateARSmoothing(const float TimeStamp, TConstArrayView<float> SquaredMagnitudes)
	{
		// Calculate AR smoother coefficients:
		const float DeltaT = (TimeStamp - PrevTimeStamp.GetValue());
		Audio::FAttackRelease AttackRelease(1.0f / DeltaT, AttackTimeMsec, ReleaseTimeMsec, bIsAnalogAttackRelease);

		// Apply AR smoothing for each frequency:
		check(SquaredMagnitudes.Num() == ARSmoothedSquaredMagnitudes.Num());
		for (int Index = 0; Index < SquaredMagnitudes.Num(); Index++)
		{
			const float OldValue = ARSmoothedSquaredMagnitudes[Index];
			const float NewValue = SquaredMagnitudes[Index];
			const float ARSmootherCoefficient = (NewValue >= OldValue) ? AttackRelease.GetAttackTimeSamples() : AttackRelease.GetReleaseTimeSamples();
			ARSmoothedSquaredMagnitudes[Index] = FMath::Lerp(NewValue, OldValue, ARSmootherCoefficient);
		}
	}

	void FAudioSpectrumAnalyzer::Teardown()
	{
		if (SpectrumAnalyzer.IsValid() && SpectrumAnalyzer->IsValidLowLevel())
		{
			if (AnalyzerType == EAudioSpectrumAnalyzerType::FFT)
			{
				SpectrumAnalyzer->StopAnalyzing();
			}

			ReleaseSynesthesiaSpectrumAnalyzer();
		}

		if (ConstantQAnalyzer.IsValid() && ConstantQAnalyzer->IsValidLowLevel())
		{
			if (AnalyzerType == EAudioSpectrumAnalyzerType::CQT)
			{
				ConstantQAnalyzer->StopAnalyzing();
			}

			ReleaseConstantQAnalyzer();
		}

		PrevTimeStamp.Reset();
		CenterFrequencies.Empty();
		ARSmoothedSquaredMagnitudes.Empty();

		AudioBus.Reset();
		bUseExternalAudioBus = false;
	}

	FAudioPowerSpectrumData FAudioSpectrumAnalyzer::GetAudioSpectrumData() const
	{
		check(CenterFrequencies.Num() == ARSmoothedSquaredMagnitudes.Num());
		return FAudioPowerSpectrumData{ CenterFrequencies, ARSmoothedSquaredMagnitudes };
	}

	void FAudioSpectrumAnalyzer::ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("AnalyzerSettings", LOCTEXT("AnalyzerSettings", "Analyzer Settings"));
		MenuBuilder.AddSubMenu(
			LOCTEXT("Ballistics", "Ballistics"),
			FText(),
			FNewMenuDelegate::CreateSP(this, &FAudioSpectrumAnalyzer::BuildBallisticsSubMenu));
		MenuBuilder.AddSubMenu(
			LOCTEXT("AnalyzerType", "Analyzer Type"),
			FText(),
			FNewMenuDelegate::CreateSP(this, &FAudioSpectrumAnalyzer::BuildAnalyzerTypeSubMenu));
		MenuBuilder.AddSubMenu(
			LOCTEXT("FFTSize", "FFT Size"),
			FText(),
			FNewMenuDelegate::CreateSP(this, &FAudioSpectrumAnalyzer::BuildFFTSizeSubMenu));
		MenuBuilder.EndSection();
	}

	void FAudioSpectrumAnalyzer::BuildBallisticsSubMenu(FMenuBuilder& SubMenu)
	{
		SubMenu.AddMenuEntry(
			LOCTEXT("Analog", "Analog"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this]() { bIsAnalogAttackRelease = true; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]() { return bIsAnalogAttackRelease; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
		SubMenu.AddMenuEntry(
			LOCTEXT("Digital", "Digital"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this]() { bIsAnalogAttackRelease = false; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]() { return !bIsAnalogAttackRelease; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}

	void FAudioSpectrumAnalyzer::BuildAnalyzerTypeSubMenu(FMenuBuilder& SubMenu)
	{
		const UEnum* EnumClass = StaticEnum<EAudioSpectrumAnalyzerType>();
		const int32 NumEnumValues = EnumClass->NumEnums() - 1; // Exclude 'MAX' enum value.
		for (int32 Index = 0; Index < NumEnumValues; Index++)
		{
			const auto EnumValue = static_cast<EAudioSpectrumAnalyzerType>(EnumClass->GetValueByIndex(Index));

			SubMenu.AddMenuEntry(
				EnumClass->GetDisplayNameTextByIndex(Index),
#if WITH_EDITOR
				EnumClass->GetToolTipTextByIndex(Index),
#else
				FText(),
#endif
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAudioSpectrumAnalyzer::SetAnalyzerType, EnumValue),
					FCanExecuteAction(),
					FIsActionChecked::CreateSPLambda(this, [this, EnumValue]() { return (AnalyzerType == EnumValue); })
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
		}
	}

	void FAudioSpectrumAnalyzer::BuildFFTSizeSubMenu(FMenuBuilder& SubMenu)
	{
		// There is a different FFTSize enum depending on the analyzer type.

		if (AnalyzerType == EAudioSpectrumAnalyzerType::FFT)
		{
			const UEnum* EnumClass = StaticEnum<EFFTSize>();
			const int32 NumEnumValues = EnumClass->NumEnums() - 1; // Exclude 'MAX' enum value.
			for (int32 Index = 0; Index < NumEnumValues; Index++)
			{
				const auto EnumValue = static_cast<EFFTSize>(EnumClass->GetValueByIndex(Index));
				if (EnumValue == EFFTSize::DefaultSize)
				{
					// Skip the duplicate 512 enum value 'DefaultSize'.
					continue;
				}

				SubMenu.AddMenuEntry(
					EnumClass->GetDisplayNameTextByIndex(Index),
#if WITH_EDITOR
					EnumClass->GetToolTipTextByIndex(Index),
#else
					FText(),
#endif
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAudioSpectrumAnalyzer::SetSynesthesiaSpectrumAnalyzerFFTSize, EnumValue),
						FCanExecuteAction(),
						FIsActionChecked::CreateSPLambda(this, [this, EnumValue]() { return (SpectrumAnalysisSettings->FFTSize == EnumValue); })
					),
					NAME_None,
					EUserInterfaceActionType::ToggleButton);
			}
		}
		else if (AnalyzerType == EAudioSpectrumAnalyzerType::CQT)
		{
			const UEnum* EnumClass = StaticEnum<EConstantQFFTSizeEnum>();
			const int32 NumEnumValues = EnumClass->NumEnums() - 1; // Exclude 'MAX' enum value.
			for (int32 Index = 0; Index < NumEnumValues; Index++)
			{
				const auto EnumValue = static_cast<EConstantQFFTSizeEnum>(EnumClass->GetValueByIndex(Index));

				SubMenu.AddMenuEntry(
					EnumClass->GetDisplayNameTextByIndex(Index),
#if WITH_EDITOR
					EnumClass->GetToolTipTextByIndex(Index),
#else
					FText(),
#endif
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAudioSpectrumAnalyzer::SetConstantQAnalyzerFFTSize, EnumValue),
						FCanExecuteAction(),
						FIsActionChecked::CreateSPLambda(this, [this, EnumValue]() { return (ConstantQSettings->FFTSize == EnumValue); })
					),
					NAME_None,
					EUserInterfaceActionType::ToggleButton);
			}
		}
	}

	void FAudioSpectrumAnalyzer::SetAnalyzerType(const EAudioSpectrumAnalyzerType InAnalyzerType)
	{
		if (AnalyzerType != InAnalyzerType)
		{
			StopAnalyzing();

			AnalyzerType = InAnalyzerType;

			PrevTimeStamp.Reset();
			CenterFrequencies.Reset();
			ARSmoothedSquaredMagnitudes.Reset();

			StartAnalyzing();
		}
	}

	void FAudioSpectrumAnalyzer::SetSynesthesiaSpectrumAnalyzerFFTSize(const EFFTSize FFTSize)
	{
		if (SpectrumAnalysisSettings->FFTSize != FFTSize)
		{
			StopAnalyzing();
			ReleaseSynesthesiaSpectrumAnalyzer();

			SpectrumAnalysisSettings->FFTSize = FFTSize;

			CreateSynesthesiaSpectrumAnalyzer();
			StartAnalyzing();
		}
	}

	void FAudioSpectrumAnalyzer::SetConstantQAnalyzerFFTSize(const EConstantQFFTSizeEnum FFTSize)
	{
		if (ConstantQSettings->FFTSize != FFTSize)
		{
			StopAnalyzing();
			ReleaseConstantQAnalyzer();

			ConstantQSettings->FFTSize = FFTSize;

			CreateConstantQAnalyzer();
			StartAnalyzing();
		}
	}

	void FAudioSpectrumAnalyzer::CreateSynesthesiaSpectrumAnalyzer()
	{
		ensure(!SpectrumAnalyzer.IsValid());
		ensure(!SpectrumResultsDelegateHandle.IsValid());

		SpectrumAnalyzer = TStrongObjectPtr(NewObject<USynesthesiaSpectrumAnalyzer>());
		SpectrumAnalyzer->Settings = SpectrumAnalysisSettings.Get();
		SpectrumResultsDelegateHandle = SpectrumAnalyzer->OnSpectrumResultsNative.AddRaw(this, &FAudioSpectrumAnalyzer::OnSpectrumResults);
	}

	void FAudioSpectrumAnalyzer::ReleaseSynesthesiaSpectrumAnalyzer()
	{
		if (ensure(SpectrumAnalyzer.IsValid() && SpectrumResultsDelegateHandle.IsValid()))
		{
			SpectrumAnalyzer->OnSpectrumResultsNative.Remove(SpectrumResultsDelegateHandle);
		}

		SpectrumResultsDelegateHandle.Reset();
		SpectrumAnalyzer.Reset();
	}

	void FAudioSpectrumAnalyzer::CreateConstantQAnalyzer()
	{
		ConstantQAnalyzer = TStrongObjectPtr(NewObject<UConstantQAnalyzer>());
		ConstantQAnalyzer->Settings = ConstantQSettings.Get();
		ConstantQResultsDelegateHandle = ConstantQAnalyzer->OnConstantQResultsNative.AddRaw(this, &FAudioSpectrumAnalyzer::OnConstantQResults);
	}

	void FAudioSpectrumAnalyzer::ReleaseConstantQAnalyzer()
	{
		if (ensure(ConstantQAnalyzer.IsValid() && ConstantQResultsDelegateHandle.IsValid()))
		{
			ConstantQAnalyzer->OnConstantQResultsNative.Remove(ConstantQResultsDelegateHandle);
		}

		ConstantQResultsDelegateHandle.Reset();
		ConstantQAnalyzer.Reset();
	}

} // namespace AudioWidgets

#undef LOCTEXT_NAMESPACE
