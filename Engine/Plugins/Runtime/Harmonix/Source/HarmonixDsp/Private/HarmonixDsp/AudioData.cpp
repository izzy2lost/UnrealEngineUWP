// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/AudioData.h"

DEFINE_LOG_CATEGORY(LogHarmonixAudioData);

namespace HarmonixDsp
{
	static FSharedProxyDataMap ProxyToDataMap;
}

//HarmonixDsp::FSharedProxyDataMap HarmonixDsp::IAudioData::ProxyToDataMap;

FCriticalSection HarmonixDsp::IAudioData::MapCritSec;

HarmonixDsp::IAudioData::~IAudioData()
{
	FScopeLock Lock(&MapCritSec);

	// if MyProxyData is null, then this instance was created
	// directly instead of through the GetShared method
	if (MyProxyData.IsValid())
	{
		FSharedProxyDataRef ProxyRef = MyProxyData.ToSharedRef();
		if (FWeakAudioDataPtr* WeakData = ProxyToDataMap.Find(ProxyRef))
		{
			// if the data is _valid_, then it must be a new instance
			// don't remove the key-pair from the map since it was just added
			// by someone else with a new reference
			if (!WeakData->IsValid())
			{
				ProxyToDataMap.Remove(ProxyRef);
			}

		}
		else
		{
			// there should never be a situation where the map doesn't contain -something-
			// if so, that means IAudioData was initialized with a ProxyData ptr, 
			// but never got added to the map. Which shouldn't have happened!
			checkNoEntry();
		}
	}
}

ESpeakerMask::Type HarmonixDsp::IAudioData::GetChannelMaskForNumChannels(int32 InNumChannels)
{
	switch (InNumChannels)
	{
	case 1: return ESpeakerMask::UnspecifiedMono;
	case 2: return ESpeakerMask::Stereo;
	case 3: return ESpeakerMask::FiveDotZero;
	case 6: return ESpeakerMask::FiveDotOne;
	case 8: return ESpeakerMask::SevenDotOne;
	case 4:
		UE_LOG(LogHarmonixAudioData, Log, TEXT("4 channel .wav file will be assumed to be an ambisonic recording"));
		return ESpeakerMask::AmbisonicAssigned;
		break;
	}

	// Invalid num channels
	checkNoEntry();
	return ESpeakerMask::AllSpeakers;
}

HarmonixDsp::FWeakAudioDataPtr* HarmonixDsp::IAudioData::FindShared(FSharedProxyDataRef ProxyDataRef)
{
	return ProxyToDataMap.Find(ProxyDataRef);
}

void HarmonixDsp::IAudioData::AddShared(FSharedProxyDataRef ProxyDataRef, FWeakAudioDataPtr AudioData)
{
	ProxyToDataMap.Add(ProxyDataRef, AudioData);
}
