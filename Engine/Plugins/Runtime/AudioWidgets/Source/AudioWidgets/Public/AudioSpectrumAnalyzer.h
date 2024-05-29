// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ConstantQ.h"
#include "SAudioSpectrumPlot.h"
#include "Sound/AudioBus.h"
#include "SynesthesiaSpectrumAnalysis.h"
#include "UObject/StrongObjectPtr.h"

class UWorld;

UENUM(BlueprintType)
enum class EAudioSpectrumAnalyzerType : uint8
{
	FFT UMETA(ToolTip = "Fast Fourier Transform"),
	CQT UMETA(ToolTip = "Constant-Q Transform"),
};

namespace AudioWidgets
{
	/**
	 * Owns an analyzer and a corresponding Slate widget for displaying the resulting spectrum.
	 * Exponential time-smoothing is applied to the spectrum.
	 * Can either create an Audio Bus to analyze, or analyze the given Bus.
	 */
	class AUDIOWIDGETS_API FAudioSpectrumAnalyzer : public TSharedFromThis<FAudioSpectrumAnalyzer>
	{
	public:
		FAudioSpectrumAnalyzer(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus = nullptr);
		~FAudioSpectrumAnalyzer();

		UAudioBus* GetAudioBus() const;

		TSharedRef<SWidget> GetWidget() const;

		void Init(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus = nullptr);

	protected:
		void StartAnalyzing();
		void StopAnalyzing();

		void OnSpectrumResults(USynesthesiaSpectrumAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FSynesthesiaSpectrumResults>& InSpectrumResultsArray);
		void OnConstantQResults(UConstantQAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FConstantQResults>& InSpectrumResultsArray);
		void UpdateARSmoothing(const float TimeStamp, TConstArrayView<float> SquaredMagnitudes);

		FAudioPowerSpectrumData GetAudioSpectrumData() const;

		void ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder);
		void BuildBallisticsSubMenu(FMenuBuilder& SubMenu);
		void BuildAnalyzerTypeSubMenu(FMenuBuilder& SubMenu);
		void BuildFFTSizeSubMenu(FMenuBuilder& SubMenu);

		void SetAnalyzerType(const EAudioSpectrumAnalyzerType InAnalyzerType);
		void SetSynesthesiaSpectrumAnalyzerFFTSize(const EFFTSize FFTSize);
		void SetConstantQAnalyzerFFTSize(const EConstantQFFTSizeEnum FFTSize);

	private:
		void CreateSynesthesiaSpectrumAnalyzer();
		void ReleaseSynesthesiaSpectrumAnalyzer();
			
		void CreateConstantQAnalyzer();
		void ReleaseConstantQAnalyzer();

		void Teardown();

		/** Audio analyzer objects. */
		TStrongObjectPtr<USynesthesiaSpectrumAnalyzer> SpectrumAnalyzer;
		TStrongObjectPtr<UConstantQAnalyzer> ConstantQAnalyzer;

		/** The audio bus used for analysis. */
		TStrongObjectPtr<UAudioBus> AudioBus;

		/** Meaning of spectrum data. */
		TArray<float> CenterFrequencies;

		/** Cached spectrum data, with AR smoothing applied. */
		TArray<float> ARSmoothedSquaredMagnitudes;

		/** Handles for results delegate for analyzers. */
		FDelegateHandle SpectrumResultsDelegateHandle;
		FDelegateHandle ConstantQResultsDelegateHandle;

		/** Analyzer settings. */
		TStrongObjectPtr<USynesthesiaSpectrumAnalysisSettings> SpectrumAnalysisSettings;
		TStrongObjectPtr<UConstantQSettings> ConstantQSettings;

		/** Slate widget for spectrum display */
		TSharedPtr<SAudioSpectrumPlot> Widget;
		TSharedPtr<const FExtensionBase> ContextMenuExtension;

		Audio::FDeviceId AudioDeviceId = INDEX_NONE;
		bool bUseExternalAudioBus = false;

		EAudioSpectrumAnalyzerType AnalyzerType = EAudioSpectrumAnalyzerType::CQT;

		TOptional<float> PrevTimeStamp;
		float WindowCompensationPowerGain = 1.0f;
		float AttackTimeMsec = 300.0f;
		float ReleaseTimeMsec = 300.0f;
		bool bIsAnalogAttackRelease = false;
	};
} // namespace AudioWidgets
