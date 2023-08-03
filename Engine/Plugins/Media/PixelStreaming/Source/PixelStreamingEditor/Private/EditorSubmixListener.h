// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDeviceHandle.h"
#include "ISubmixBufferListener.h"
#include "IPixelStreamingAudioInput.h"

namespace UE::EditorPixelStreaming
{
	class FEditorSubmixListener : public ISubmixBufferListener
	{
	public:
		FEditorSubmixListener(FAudioDeviceHandle AudioDevice);
		virtual ~FEditorSubmixListener() = default;

		// ISubmixBufferListener interface
		void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
			int32 NumSamples, int32 NumChannels,
			const int32 SampleRate, double AudioClock) override;

	private:
		TSharedPtr<IPixelStreamingAudioInput> AudioInput;
	};
} // namespace UE::EditorPixelStreaming