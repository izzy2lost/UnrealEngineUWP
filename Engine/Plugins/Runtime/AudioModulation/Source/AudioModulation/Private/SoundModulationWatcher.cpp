// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundModulationWatcher.h"

#include "AudioDeviceHandle.h"
#include "AudioModulation.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundModulationWatcher)


namespace AudioModulation::WatcherPrivate
{
	FAudioDeviceHandle GetAudioDevice(const USoundModulationWatcher& InWatcher)
	{
		if (GEngine)
		{
			if (GEngine->UseSound())
			{
				UWorld* World = GEngine->GetWorldFromContextObject(&InWatcher, EGetWorldErrorMode::ReturnNull);
				if (World && World->bAllowAudioPlayback && !World->IsNetMode(NM_DedicatedServer))
				{
					return World->GetAudioDevice();
				}
				else
				{
					return GEngine->GetMainAudioDevice();
				}
			}
		}

		return { };
	}

	FAudioModulationManager* GetModulationManager(const USoundModulationWatcher& InWatcher)
	{
		FAudioDeviceHandle AudioDevice = WatcherPrivate::GetAudioDevice(InWatcher);
		if (AudioDevice.IsValid() && AudioDevice->IsModulationPluginEnabled())
		{
			if (IAudioModulationManager* ModulationInterface = AudioDevice->ModulationInterface.Get())
			{
				return static_cast<FAudioModulationManager*>(ModulationInterface);
			}
		}

		return nullptr;
	}
} // AudioModulation::WatcherPrivate

bool USoundModulationWatcher::ClearModulator()
{
	if (Modulator)
	{
		Modulator = nullptr;
		Destination.UpdateModulators(TSet<const USoundModulatorBase*>{ });
		return true;
	}

	return false;
}

const USoundModulatorBase* USoundModulationWatcher::GetModulator() const
{
	return Modulator;
}

float USoundModulationWatcher::GetValue() const
{
	using namespace AudioModulation;

	if (Modulator)
	{
		if (FAudioModulationManager* Modulation = WatcherPrivate::GetModulationManager(*this))
		{
			return Modulation->GetModulatorValueThreadSafe(Modulator->GetUniqueID());
		}
	}

	return 1.0f;
}

void USoundModulationWatcher::PostInitProperties()
{
	using namespace AudioModulation;

	Super::PostInitProperties();

	if (USoundModulationWatcher::StaticClass()->GetDefaultObject() == this)
	{
		return;
	}

	FAudioDeviceHandle AudioDevice = WatcherPrivate::GetAudioDevice(*this);
	if (AudioDevice.IsValid())
	{
		constexpr bool bIsBuffered = false;
		Destination.Init(AudioDevice.GetDeviceID(), bIsBuffered);
	}
}

bool USoundModulationWatcher::SetModulator(const USoundModulatorBase* InModulator)
{
	if (InModulator == Modulator)
	{
		return true;
	}

	if (InModulator)
	{
		Modulator = InModulator;
		Destination.UpdateModulators(TSet<const USoundModulatorBase*>{ InModulator });
		return true;
	}

	return ClearModulator();
}
