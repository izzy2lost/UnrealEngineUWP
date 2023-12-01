// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioOscilloscopeAnalyzer.h"

#include "AudioBusSubsystem.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioOscilloscope.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"
#include "Editor.h"

namespace UE::Audio::Insights
{
	namespace FAudioOscilloscopeAnalyzerPrivate
	{
		TSharedRef<AudioWidgets::FAudioOscilloscope> CreateAudioOscilloscope(const float InTimeWindowMs,
			const float InMaxTimeWindowMs,
			const float InAnalysisPeriodMs,
			const EAudioPanelLayoutType InPanelLayoutType)
		{
			using namespace ::Audio;

			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
				TArray<UWorld*> DeviceWorlds = AudioDeviceManager->GetWorldsUsingAudioDevice(InsightsModule.GetDeviceId());

				if (UWorld* World = (DeviceWorlds.Num() > 0) ? DeviceWorlds[0] : nullptr)
				{
					if (FMixerDevice* MixerDevice = static_cast<FMixerDevice*>(World->GetAudioDeviceRaw()))
					{
						return MakeShared<AudioWidgets::FAudioOscilloscope>(DeviceWorlds[0],
							MixerDevice->GetNumDeviceChannels(),
							InTimeWindowMs,
							InMaxTimeWindowMs,
							InAnalysisPeriodMs,
							InPanelLayoutType);
					}
				}
			}
			
			return MakeShared<AudioWidgets::FAudioOscilloscope>(GEditor->GetWorld(), 1, InTimeWindowMs, InMaxTimeWindowMs, InAnalysisPeriodMs, InPanelLayoutType);
		}
	}

	FAudioOscilloscopeAnalyzer::FAudioOscilloscopeAnalyzer(TWeakObjectPtr<USoundSubmix> InSoundSubmix)
		: AudioOscilloscope(FAudioOscilloscopeAnalyzerPrivate::CreateAudioOscilloscope(TimeWindowMs, MaxTimeWindowMs, AnalysisPeriodMs, PanelLayoutType))
	{
		RebuildAudioOscilloscope(InSoundSubmix);
	}

	FAudioOscilloscopeAnalyzer::~FAudioOscilloscopeAnalyzer()
	{
		CleanupAudioOscilloscope();
	}

	void FAudioOscilloscopeAnalyzer::RebuildAudioOscilloscope(TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		using namespace ::Audio;

		if (SoundSubmix.IsValid())
		{
			CleanupAudioOscilloscope();
		}

		SoundSubmix = InSoundSubmix;

		FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
		if (!AudioDeviceManager)
		{
			return;
		}

		const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
		TArray<UWorld*> DeviceWorlds = AudioDeviceManager->GetWorldsUsingAudioDevice(InsightsModule.GetDeviceId());

		UWorld* World = (DeviceWorlds.Num() > 0) ? DeviceWorlds[0] : nullptr;
		if (!World)
		{
			return;
		}

		FMixerDevice* MixerDevice = static_cast<FMixerDevice*>(World->GetAudioDeviceRaw());
		if (!MixerDevice)
		{
			return;
		}

		if (!SoundSubmix.IsValid())
		{
			return;
		}

		FMixerSubmixPtr MixerSubmix = MixerDevice->GetSubmixInstance(SoundSubmix.Get()).Pin();
		if (!MixerSubmix.IsValid())
		{
			return;
		}

		AudioOscilloscope->CreateDataProvider(World, TimeWindowMs, MaxTimeWindowMs, AnalysisPeriodMs, PanelLayoutType);
		AudioOscilloscope->CreateOscilloscopeWidget(MixerDevice->GetNumDeviceChannels(), PanelLayoutType);

		// Start processing
		AudioOscilloscope->StartProcessing();
		
		// Register audio bus in submix
		const TObjectPtr<UAudioBus> AudioBus = AudioOscilloscope->GetAudioBus();
		if (!AudioBus)
		{
			return;
		}

		const FAudioBusKey AudioBusKey(AudioBus->GetUniqueID());
		const int32 AudioBusNumChannels = AudioBus->GetNumChannels();

		FAudioThread::RunCommandOnAudioThread([MixerDevice, MixerSubmix, AudioBusKey, AudioBusNumChannels]()
		{
			TObjectPtr<UAudioBusSubsystem> AudioBusSubsystem = MixerDevice->GetSubsystem<UAudioBusSubsystem>();
			check(AudioBusSubsystem);

			MixerSubmix->RegisterAudioBus(AudioBusKey, AudioBusSubsystem->AddPatchInputForAudioBus(AudioBusKey, MixerDevice->GetNumOutputFrames(), AudioBusNumChannels));
		});
	}

	void FAudioOscilloscopeAnalyzer::CleanupAudioOscilloscope()
	{
		using namespace ::Audio;

		FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
		if (!AudioDeviceManager)
		{
			return;
		}

		const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
		TArray<UWorld*> DeviceWorlds = AudioDeviceManager->GetWorldsUsingAudioDevice(InsightsModule.GetDeviceId());

		UWorld* World = (DeviceWorlds.Num() > 0) ? DeviceWorlds[0] : nullptr;
		if (!World)
		{
			return;
		}

		FMixerDevice* MixerDevice = static_cast<FMixerDevice*>(World->GetAudioDeviceRaw());
		if (!MixerDevice)
		{
			return;
		}

		if (!SoundSubmix.IsValid())
		{
			return;
		}

		FMixerSubmixPtr MixerSubmix = MixerDevice->GetSubmixInstance(SoundSubmix.Get()).Pin();
		if (!MixerSubmix.IsValid())
		{
			return;
		}

		// Unregister audio bus from submix
		const TObjectPtr<UAudioBus> AudioBus = AudioOscilloscope->GetAudioBus();
		if (!AudioBus)
		{
			return;
		}

		const FAudioBusKey AudioBusKey(AudioBus->GetUniqueID());

		FAudioThread::RunCommandOnAudioThread([MixerDevice, MixerSubmix, AudioBusKey]()
		{
			MixerSubmix->UnregisterAudioBus(AudioBusKey);
		});

		// Stop processing
		AudioOscilloscope->StopProcessing();

		SoundSubmix.Reset();
	}
} // namespace UE::Audio::Insights
