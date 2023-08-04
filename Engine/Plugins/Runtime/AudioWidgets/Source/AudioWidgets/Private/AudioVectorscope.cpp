// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioVectorscope.h"

#include "Engine/World.h"
#include "SAudioVectorscopePanelWidget.h"

namespace AudioWidgets
{
	FAudioVectorscope::FAudioVectorscope(UWorld* InWorld, 
		const uint32 InNumChannels, 
		const float InTimeWindowMs, 
		const float InMaxTimeWindowMs, 
		const float InAnalysisPeriodMs, 
		const EAudioPanelLayoutType InPanelLayoutType)
		: VectorscopePanelStyle(FAudioVectorscopePanelStyle::GetDefault())
	{
		// Init audio bus
		AudioBus = TStrongObjectPtr(NewObject<UAudioBus>());
		AudioBus->AudioBusChannels = AudioBusUtils::ConvertIntToEAudioBusChannels(InNumChannels);

		// Init data provider
		AudioSamplesDataProvider = MakeShared<FWaveformAudioSamplesDataProvider>(InWorld, AudioBus.Get(), AudioBus->GetNumChannels(), InTimeWindowMs, InMaxTimeWindowMs, InAnalysisPeriodMs);

		// Init widget 
		const FFixedSampledSequenceView SequenceView = AudioSamplesDataProvider->GetDataView();

		VectorscopePanelWidget = SNew(SAudioVectorscopePanelWidget, SequenceView)
		.PanelLayoutType(InPanelLayoutType)
		.PanelStyle(&VectorscopePanelStyle);

		// Interconnect data provider and widget
		AudioSamplesDataProvider->OnDataViewGenerated.AddSP(VectorscopePanelWidget.Get(), &SAudioVectorscopePanelWidget::ReceiveSequenceView);

		if (InPanelLayoutType == EAudioPanelLayoutType::Advanced)
		{
			VectorscopePanelWidget->OnTimeWindowValueChanged.AddSP(AudioSamplesDataProvider.Get(), &FWaveformAudioSamplesDataProvider::SetTimeWindow);
		}
	}

	void FAudioVectorscope::StartProcessing()
	{
		AudioSamplesDataProvider->StartProcessing();
	}

	void FAudioVectorscope::StopProcessing()
	{
		AudioSamplesDataProvider->StopProcessing();
	}

	UAudioBus* FAudioVectorscope::GetAudioBus() const
	{
		return AudioBus.Get();
	}

	TSharedRef<SWidget> FAudioVectorscope::GetPanelWidget() const
	{
		return VectorscopePanelWidget.ToSharedRef();
	}
} // namespace AudioWidgets
