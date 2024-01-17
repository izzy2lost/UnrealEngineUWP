// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/AudioBufferConstants.h"

#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "HAL/Platform.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHarmonixAudioData, Log, All);

namespace Audio
{
	class IProxyData;
};

enum class EAudioEncodedFormat : uint8
{
	PCM,
	Float32,
	Num,
	Invalid
};

namespace HarmonixDsp
{
	class IAudioData;

	using FSharedAudioDataRef = TSharedRef<IAudioData, ESPMode::ThreadSafe>;
	using FSharedAudioDataPtr = TSharedPtr<IAudioData, ESPMode::ThreadSafe>;
	
	using FSharedProxyDataRef = TSharedRef<Audio::IProxyData, ESPMode::ThreadSafe>;
	using FSharedProxyDataPtr = TSharedPtr<Audio::IProxyData, ESPMode::ThreadSafe>;

	using FWeakAudioDataPtr = TWeakPtr<IAudioData, ESPMode::ThreadSafe>;

	using FSharedProxyDataMap = TMap<FSharedProxyDataRef, FWeakAudioDataPtr>;

	/**
	 *
	 * When creating IAudioData, you want to call
	 *
	 *    IAudioData::GetShared<TDataType, TProxyType>(MyAudioData, ProxyData);
	 *
	 * This will return a ptr to a Existing Audio Data if it has aleady been loaded instead of creating a new one.
	 *
	 * This works by mapping the proxy data to the audio data
	 *
	 *    TSharedRef<IAudioProxy> --> TWeakPtr<IAudioData>
	 *
	 * This assumes there's a 1-1 relationship between AudioProxy and AudioData (which I think is a safe assumption)
	 *
	 * The mapping is to a WeakPtr of IAudioData. That way, when all shared references to the IAudioData go away,
	 * the destructor of ~IAudioData gets called and will automatically remove itself from the SharedAudioData mapping
	 *
	 * For example:
	 *
	 *     FMyAudioData::GetShared<FMyAudioData, FProxyData>(ProxyData.ToSharedRef(), Args...);
	 *
	 * any additional args that are used for the construction of a new audio data instance won't be applied to existing audio data instances.
	 */
	class HARMONIXDSP_API IAudioData : public TSharedFromThis<IAudioData>
	{
	public:

		virtual const FName& GetName() const = 0;
		virtual EAudioEncodedFormat GetFormat() const { return EAudioEncodedFormat::Invalid; }
		virtual bool IsStreaming() const = 0;

		virtual bool GetHasTailSection() const = 0;
		virtual bool GetHasLoopSection() const = 0;
		virtual uint32 GetLoopStartFrame() const = 0;
		virtual uint32 GetLoopEndFrame() const = 0;

		virtual float GetSampleRate() const = 0;
		virtual uint32 GetNumFrames() const = 0;
		virtual int32 GetNumChannels() const = 0;
		virtual float GetDurationSeconds() const = 0;

		// what speaker channels are represented in the data?
		virtual uint32 GetChannelMask() const = 0;

		// what speaker layout was this authored for?
		virtual EAudioBufferChannelLayout GetChannelLayout() const = 0;

		virtual bool Failed() const = 0;

		virtual ~IAudioData();

		static ESpeakerMask::Type GetChannelMaskForNumChannels(int32 InNumChannels);

	public:

		// You _must_ implement the constructor of your audio data 
		// so that its first argument is a TSharedRef to the ProxyData it wraps
		template<typename TDataType, typename TProxyType, typename... Args>
		static TSharedPtr<TDataType, ESPMode::ThreadSafe> GetShared(TSharedRef<TProxyType, ESPMode::ThreadSafe> ProxyDataRef, Args... InArgs)
		{
			FScopeLock Lock(&MapCritSec);

			if (FWeakAudioDataPtr* WeakData = FindShared(ProxyDataRef))
			{
				// if the weak ptr is null, then the previous reference to this data is being destroyed
				// but hasn't been removed from the map yet. We'll have to add a new reference.
				if (FSharedAudioDataPtr SharedData = WeakData->Pin())
				{
					TSharedPtr<TDataType, ESPMode::ThreadSafe> OutData = StaticCastSharedPtr<TDataType>(SharedData);
					// There a reference to AudioData, and it _is_ valid. 
					// Check that it's the type we asked for!
					check(OutData);
					return OutData;
				}
			}

			TSharedPtr<TDataType, ESPMode::ThreadSafe> NewData = MakeShared<TDataType>(ProxyDataRef, Forward<Args>(InArgs)...);
			NewData->MyProxyData = ProxyDataRef.ToSharedPtr();
			AddShared(ProxyDataRef, NewData.ToWeakPtr());
			return NewData;
		}

	private:

		// These methods are non-locking, so you need to lock in the outer scope
		static FWeakAudioDataPtr* FindShared(FSharedProxyDataRef ProxyDataRef);
		static void AddShared(FSharedProxyDataRef ProxyDataRef, FWeakAudioDataPtr AudioData);

		// Have a back ptr to the mapped Proxy Data so we can use it to index into the TMap
		FSharedProxyDataPtr MyProxyData;

		
		static FCriticalSection MapCritSec;
	};


	
}

