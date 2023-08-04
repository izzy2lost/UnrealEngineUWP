// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioOscilloscope.h"

#include "Engine/World.h"

namespace AudioWidgets
{
	FAudioOscilloscope::FAudioOscilloscope(UWorld* InWorld, 
		const uint32 InNumChannels, 
		const float InTimeWindowMs, 
		const float InMaxTimeWindowMs, 
		const float InAnalysisPeriodMs, 
		const EAudioPanelLayoutType InPanelLayoutType)
		: OscilloscopePanelStyle(FAudioOscilloscopePanelStyle::GetDefault())
	{
		// Init audio bus
		AudioBus = TStrongObjectPtr(NewObject<UAudioBus>());
		AudioBus->AudioBusChannels = AudioBusUtils::ConvertIntToEAudioBusChannels(InNumChannels);

		// Init data provider
		constexpr uint32 NumChannelsToProvide = 1;
		AudioSamplesDataProvider = MakeShared<FWaveformAudioSamplesDataProvider>(InWorld, AudioBus.Get(), NumChannelsToProvide, InTimeWindowMs, InMaxTimeWindowMs, InAnalysisPeriodMs);

		// Init widget 
		const FFixedSampledSequenceView SequenceView = AudioSamplesDataProvider->GetDataView();

		OscilloscopePanelWidget = SNew(SAudioOscilloscopePanelWidget, SequenceView, InNumChannels)
		.PanelLayoutType(InPanelLayoutType)
		.PanelStyle(&OscilloscopePanelStyle);

		// Interconnect data provider and widget
		AudioSamplesDataProvider->OnDataViewGenerated.AddSP(OscilloscopePanelWidget.Get(), &SAudioOscilloscopePanelWidget::ReceiveSequenceView);

		if (InPanelLayoutType == EAudioPanelLayoutType::Advanced)
		{
			OscilloscopePanelWidget->OnSelectedChannelChanged.AddSP(AudioSamplesDataProvider.Get(),  &FWaveformAudioSamplesDataProvider::SetChannelToAnalyze);
			OscilloscopePanelWidget->OnTriggerModeChanged.AddSP(AudioSamplesDataProvider.Get(),      &FWaveformAudioSamplesDataProvider::SetTriggerMode);
			OscilloscopePanelWidget->OnTriggerThresholdChanged.AddSP(AudioSamplesDataProvider.Get(), &FWaveformAudioSamplesDataProvider::SetTriggerThreshold);
			OscilloscopePanelWidget->OnTimeWindowValueChanged.AddSP(AudioSamplesDataProvider.Get(),  &FWaveformAudioSamplesDataProvider::SetTimeWindow);
			OscilloscopePanelWidget->OnAnalysisPeriodChanged.AddSP(AudioSamplesDataProvider.Get(),   &FWaveformAudioSamplesDataProvider::SetAnalysisPeriod);
		}
	}

	void FAudioOscilloscope::StartProcessing()
	{
		AudioSamplesDataProvider->StartProcessing();
	}

	void FAudioOscilloscope::StopProcessing()
	{
		AudioSamplesDataProvider->StopProcessing();
	}

	UAudioBus* FAudioOscilloscope::GetAudioBus() const
	{
		return AudioBus.Get();
	}

	TSharedRef<SWidget> FAudioOscilloscope::GetPanelWidget() const
	{
		return OscilloscopePanelWidget.ToSharedRef();
	}
}
