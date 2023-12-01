// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMeterAnalyzer.h"

#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioMeter.h"
#include "AudioMixerDevice.h"
#include "Editor.h"

namespace UE::Audio::Insights
{
	namespace FAudioMeterAnalyzerPrivate
	{
		TSharedRef<AudioWidgets::FAudioMeter> CreateAudioMeter(TWeakObjectPtr<UAudioBus> InExternalAudioBus)
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
						return MakeShared<AudioWidgets::FAudioMeter>(InExternalAudioBus.IsValid() ? InExternalAudioBus->GetNumChannels() : MixerDevice->GetNumDeviceChannels(), 
							*DeviceWorlds[0], 
							InExternalAudioBus.Get());
					}
				}
			}
			
			return MakeShared<AudioWidgets::FAudioMeter>(1, *GEditor->GetWorld());
		}
	}

	FAudioMeterAnalyzer::FAudioMeterAnalyzer(TWeakObjectPtr<UAudioBus> InExternalAudioBus)
		: AudioMeter(FAudioMeterAnalyzerPrivate::CreateAudioMeter(InExternalAudioBus))
	{
		
	}

	void FAudioMeterAnalyzer::RebuildAudioMeter(TWeakObjectPtr<UAudioBus> InExternalAudioBus)
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

		AudioMeter->Init(InExternalAudioBus.IsValid() ? InExternalAudioBus->GetNumChannels() : MixerDevice->GetNumDeviceChannels(), *World, InExternalAudioBus.Get());
	}
} // namespace UE::Audio::Insights
