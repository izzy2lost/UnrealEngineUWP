// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorSubmixListener.h"

#include "AudioDevice.h"
#include "PixelStreamingPeerConnection.h"

namespace UE::EditorPixelStreaming
{
	FEditorSubmixListener::FEditorSubmixListener(FAudioDeviceHandle AudioDevice)
		: AudioInput(FPixelStreamingPeerConnection::CreateAudioInput())
	{
		AudioDevice->RegisterSubmixBufferListener(this);
	}

	void FEditorSubmixListener::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
		int32 NumSamples, int32 NumChannels,
		const int32 SampleRate, double AudioClock)
	{
		AudioInput->PushAudio(AudioData, NumSamples, NumChannels, SampleRate);
	}

} // namespace UE::EditorPixelStreaming
