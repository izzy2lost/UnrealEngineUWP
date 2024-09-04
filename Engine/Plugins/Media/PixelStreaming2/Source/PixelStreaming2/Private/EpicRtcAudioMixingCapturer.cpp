// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcAudioMixingCapturer.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "PixelStreaming2PluginSettings.h"

namespace UE::PixelStreaming2
{
	FEpicRtcAudioMixingCapturer::FEpicRtcAudioMixingCapturer()
		: SampleRate(48000)
		, NumChannels(2)
		, SampleSizeSeconds(0.5f)
	{
		// subscribe to audio data
		if (!GEngine)
		{
			return;
		}

		FAudioDeviceHandle EngineAudioDevice = GEngine->GetMainAudioDevice();
		if (!EngineAudioDevice)
		{
			UE_LOG(LogPixelStreaming2, Warning, TEXT("No main audio device"));
			return;
		}

		Mixer = MakeShared<FEpicRtcAudioPatchMixer>(NumChannels, SampleRate, SampleSizeSeconds);
		MixerTask = FEpicRtcTickableTask::Create<FEpicRtcMixAudioTask>(this, Mixer);

		CreateAudioProducer(EngineAudioDevice.GetDeviceID());
	}

	TSharedPtr<FEpicRtcAudioMixingCapturer> FEpicRtcAudioMixingCapturer::Create()
	{
		TSharedPtr<FEpicRtcAudioMixingCapturer> AudioMixingCapturer(new FEpicRtcAudioMixingCapturer());

		FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddSP(AudioMixingCapturer.ToSharedRef(), &FEpicRtcAudioMixingCapturer::CreateAudioProducer);
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddSP(AudioMixingCapturer.ToSharedRef(), &FEpicRtcAudioMixingCapturer::RemoveAudioProducer);

		return AudioMixingCapturer;
	}

	TSharedPtr<FEpicRtcAudioProducer> FEpicRtcAudioMixingCapturer::CreateAudioProducer()
	{
		// The lifetimes of audio producers created by the user are the responsibility of the user
		return FEpicRtcAudioProducer::Create(MakeShared<FEpicRtcPatchInputProxy>(Mixer));
	}

	void FEpicRtcAudioMixingCapturer::CreateAudioProducer(Audio::FDeviceId AudioDeviceId)
	{
		// The lifetimes of audio producers created by the engine are our responsibility
		TSharedPtr<FEpicRtcAudioProducer> AudioInput = FEpicRtcAudioProducer::Create(AudioDeviceId, MakeShared<FEpicRtcPatchInputProxy>(Mixer));
		AudioProducers.Add(AudioDeviceId, AudioInput);
	}

	void FEpicRtcAudioMixingCapturer::RemoveAudioProducer(Audio::FDeviceId AudioDeviceId)
	{
		AudioProducers.Remove(AudioDeviceId);
	}

	void FEpicRtcAudioMixingCapturer::PushAudio(const float* AudioData, int32 InNumSamples, int32 InNumChannels, const int32 InSampleRate)
	{
		TArray<float> AudioBuffer;

		if (SampleRate != InSampleRate)
		{
			float SampleRateConversionRatio = static_cast<float>(SampleRate) / static_cast<float>(InSampleRate);
			Resampler.Init(Audio::EResamplingMethod::Linear, SampleRateConversionRatio, InNumChannels);

			int32 NumConvertedSamples = InSampleRate / SampleRateConversionRatio;
			int32 OutputSamples = INDEX_NONE;
			AudioBuffer.AddZeroed(NumConvertedSamples);
			// Perform the sample rate conversion
			int32 ErrorCode = Resampler.ProcessAudio(const_cast<float*>(AudioData), InNumSamples, false, AudioBuffer.GetData(), AudioBuffer.Num(), OutputSamples);
			verifyf(OutputSamples <= NumConvertedSamples, TEXT("OutputSamples > NumConvertedSamples"));
			if (ErrorCode != 0)
			{
				UE_LOG(LogPixelStreaming2, Warning, TEXT("(FAudioInput) Problem occured resampling audio data. Code: %d"), ErrorCode);
				return;
			}
		}
		else
		{
			AudioBuffer.AddZeroed(InNumSamples);
			FMemory::Memcpy(AudioBuffer.GetData(), AudioData, AudioBuffer.Num() * sizeof(float));
		}

		// Note: TSampleBuffer takes in AudioData as float* and internally converts to int16
		Audio::TSampleBuffer<int16_t> Buffer(AudioBuffer.GetData(), AudioBuffer.Num(), InNumChannels, InSampleRate);
		// Mix to stereo if required, since PixelStreaming2 only accepts stereo at the moment
		if (NumChannels != Buffer.GetNumChannels())
		{
			Buffer.MixBufferToChannels(NumChannels);
		}

		// Apply gain
		float Gain = UPixelStreaming2PluginSettings::CVarWebRTCAudioGain.GetValueOnAnyThread();
		if (Gain != 1.0f)
		{
			int16* PCMAudio = Buffer.GetArrayView().GetData();

			// multiply audio by gain multiplier
			for (int i = 0; i < Buffer.GetNumSamples(); i++)
			{
				*PCMAudio = FMath::Max(-32768, FMath::Min(32767, *PCMAudio * Gain));
				PCMAudio++;
			}
		}

		RecordingBuffer.Append(Buffer.GetData(), Buffer.GetNumSamples());

		const int32 SamplesPer10Ms = NumChannels * SampleRate * 0.01f;
		const size_t BytesPerFrame = NumChannels * sizeof(int16_t);

		// Feed in 10ms chunks
		while (RecordingBuffer.Num() > SamplesPer10Ms)
		{
			OnAudioBuffer.Broadcast(RecordingBuffer.GetData(), SamplesPer10Ms, NumChannels, SampleRate);

			// Remove 10ms of samples from the recording buffer now it is submitted
			RecordingBuffer.RemoveAt(0, SamplesPer10Ms, EAllowShrinking::No);
		}
	}
} // namespace UE::PixelStreaming2 