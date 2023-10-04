// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/DynamicsProcessor.h"
#include "DSP/FloatArrayMath.h"
#include "SignalProcessingModule.h"

namespace Audio
{
	FDynamicsProcessor::FDynamicsProcessor()
		: ProcessingMode(EDynamicsProcessingMode::Compressor)
		, SlopeFactor(0.0f)
		, EnvelopeFollowerPeakMode(EPeakMode::Peak)
		, LookaheedDelayMsec(10.0f)
		, AttackTimeMsec(20.0f)
		, ReleaseTimeMsec(1000.0f)
		, ThresholdDb(-6.0f)
		, Ratio(1.0f)
		, HalfKneeBandwidthDb(5.0f)
		, InputGain(1.0f)
		, OutputGain(1.0f)
		, KeyGain(1.0f)
		, LinkMode(EDynamicsProcessorChannelLinkMode::Disabled)
		, bIsAnalogMode(true)
		, bKeyAuditionEnabled(false)
		, bKeyHighshelfEnabled(false)
		, bKeyLowshelfEnabled(false)
	{
		// The knee will have 2 points
		KneePoints.Init(FKneePoint(), 2);
		CalculateSlope();
	}

	FDynamicsProcessor::~FDynamicsProcessor()
	{
	}

	void FDynamicsProcessor::Init(const float InSampleRate, const int32 InNumChannels)
	{
		SampleRate = InSampleRate;

		SetNumChannels(InNumChannels);
		SetKeyNumChannels(InNumChannels);
	
		LookaheadDelay.Reset();
		LookaheadDelay.AddDefaulted(InNumChannels);

		for (int32 Channel = 0; Channel < InNumChannels; ++Channel)
		{
			LookaheadDelay[Channel].Init(SampleRate, MaxLookaheadMsec / 1000.0f);
			LookaheadDelay[Channel].SetDelayMsec(LookaheedDelayMsec);

			EnvFollower[Channel].Init(FInlineEnvelopeFollowerInitParams{SampleRate, AttackTimeMsec, ReleaseTimeMsec, EnvelopeFollowerPeakMode, bIsAnalogMode});
		}

		InputLowshelfFilter.Init(SampleRate, InNumChannels, EBiquadFilter::LowShelf);
		InputHighshelfFilter.Init(SampleRate, InNumChannels, EBiquadFilter::HighShelf);

		DetectorOuts.Reset();
		DetectorOuts.AddZeroed(InNumChannels);

		Gain.Reset();
		Gain.AddZeroed(InNumChannels);
	}

	int32 FDynamicsProcessor::GetNumChannels() const
	{
		return Gain.Num();
	}

	int32 FDynamicsProcessor::GetKeyNumChannels() const
	{
		return EnvFollower.Num();
	}

	float Audio::FDynamicsProcessor::GetMaxLookaheadMsec() const
	{
		return MaxLookaheadMsec;
	}
	
	void FDynamicsProcessor::SetLookaheadMsec(const float InLookAheadMsec)
	{
		LookaheedDelayMsec = InLookAheadMsec;
		for (int32 Channel = 0; Channel < LookaheadDelay.Num(); ++Channel)
		{
			LookaheadDelay[Channel].SetDelayMsec(LookaheedDelayMsec);
		}
	}

	void FDynamicsProcessor::SetAttackTime(const float InAttackTimeMsec)
	{
		AttackTimeMsec = InAttackTimeMsec;
		for (int32 Channel = 0; Channel < EnvFollower.Num(); ++Channel)
		{
			EnvFollower[Channel].SetAttackTime(InAttackTimeMsec);
		}
	}

	void FDynamicsProcessor::SetReleaseTime(const float InReleaseTimeMsec)
	{
		ReleaseTimeMsec = InReleaseTimeMsec;
		for (int32 Channel = 0; Channel < EnvFollower.Num(); ++Channel)
		{
			EnvFollower[Channel].SetReleaseTime(InReleaseTimeMsec);
		}
	}

	void FDynamicsProcessor::SetThreshold(const float InThresholdDb)
	{
		ThresholdDb = InThresholdDb;
		CalculateKnee();
	}

	void FDynamicsProcessor::SetRatio(const float InCompressionRatio)
	{
		// Don't let the compression ratio be 0.0!
		Ratio = FMath::Max(InCompressionRatio, SMALL_NUMBER);
		CalculateSlope();
	}

	void FDynamicsProcessor::SetKneeBandwidth(const float InKneeBandwidthDb)
	{
		HalfKneeBandwidthDb = 0.5f * InKneeBandwidthDb;
		CalculateKnee();
	}

	void FDynamicsProcessor::SetInputGain(const float InInputGainDb)
	{
		InputGain = ConvertToLinear(InInputGainDb);
	}

	void FDynamicsProcessor::SetKeyAudition(const bool InAuditionEnabled)
	{
		bKeyAuditionEnabled = InAuditionEnabled;
	}

	void FDynamicsProcessor::SetKeyGain(const float InKeyGain)
	{
		KeyGain = ConvertToLinear(InKeyGain);
	}

	void FDynamicsProcessor::SetKeyHighshelfCutoffFrequency(const float InCutoffFreq)
	{
		InputHighshelfFilter.SetFrequency(InCutoffFreq);
	}

	void FDynamicsProcessor::SetKeyHighshelfEnabled(const bool bInEnabled)
	{
		bKeyHighshelfEnabled = bInEnabled;
	}

	void FDynamicsProcessor::SetKeyHighshelfGain(const float InGainDb)
	{
		InputHighshelfFilter.SetGainDB(InGainDb);
	}

	void FDynamicsProcessor::SetKeyLowshelfCutoffFrequency(const float InCutoffFreq)
	{
		InputLowshelfFilter.SetFrequency(InCutoffFreq);
	}

	void FDynamicsProcessor::SetKeyLowshelfEnabled(const bool bInEnabled)
	{
		bKeyLowshelfEnabled = bInEnabled;
	}

	void FDynamicsProcessor::SetKeyLowshelfGain(const float InGainDb)
	{
		InputLowshelfFilter.SetGainDB(InGainDb);
	}

	void FDynamicsProcessor::SetKeyNumChannels(const int32 InNumChannels)
	{
		if (InNumChannels != EnvFollower.Num())
		{
			EnvFollower.Reset();

			for (int32 Channel = 0; Channel < InNumChannels; ++Channel)
			{
				EnvFollower.Emplace(FInlineEnvelopeFollowerInitParams{SampleRate, AttackTimeMsec, ReleaseTimeMsec, EnvelopeFollowerPeakMode, bIsAnalogMode});
			}
		}

		if (InNumChannels != InputLowshelfFilter.GetNumChannels())
		{
			InputLowshelfFilter.Init(SampleRate, InNumChannels, EBiquadFilter::LowShelf);
		}

		if (InNumChannels != InputHighshelfFilter.GetNumChannels())
		{
			InputHighshelfFilter.Init(SampleRate, InNumChannels, EBiquadFilter::HighShelf);
		}

		if (InNumChannels != DetectorOuts.Num())
		{
			DetectorOuts.Reset();
			DetectorOuts.AddZeroed(InNumChannels);
		}
	}

	void FDynamicsProcessor::SetOutputGain(const float InOutputGainDb)
	{
		OutputGain = ConvertToLinear(InOutputGainDb);
	}

	void FDynamicsProcessor::SetChannelLinkMode(const EDynamicsProcessorChannelLinkMode InLinkMode)
	{
		LinkMode = InLinkMode;
	}

	void FDynamicsProcessor::SetAnalogMode(const bool bInIsAnalogMode)
	{
		bIsAnalogMode = bInIsAnalogMode;
		for (int32 Channel = 0; Channel < EnvFollower.Num(); ++Channel)
		{
			EnvFollower[Channel].SetAnalog(bInIsAnalogMode);
		}
	}

	void FDynamicsProcessor::SetNumChannels(const int32 InNumChannels)
	{
		if (InNumChannels != Gain.Num())
		{
			Gain.Reset();
			Gain.AddZeroed(InNumChannels);
		}

		if (InNumChannels != LookaheadDelay.Num())
		{
			LookaheadDelay.Reset();
			LookaheadDelay.AddDefaulted(InNumChannels);

			for (int32 Channel = 0; Channel < InNumChannels; ++Channel)
			{
				LookaheadDelay[Channel].Init(SampleRate, 0.1f);
				LookaheadDelay[Channel].SetDelayMsec(LookaheedDelayMsec);
			}
		}
	}

	void FDynamicsProcessor::SetPeakMode(const EPeakMode::Type InEnvelopeFollowerModeType)
	{
		EnvelopeFollowerPeakMode = InEnvelopeFollowerModeType;
		for (int32 Channel = 0; Channel < EnvFollower.Num(); ++Channel)
		{
			EnvFollower[Channel].SetMode(EnvelopeFollowerPeakMode);
		}
	}

	void FDynamicsProcessor::SetProcessingMode(const EDynamicsProcessingMode::Type InProcessingMode)
	{
		ProcessingMode = InProcessingMode;
		CalculateSlope();
	}

	void FDynamicsProcessor::ProcessAudioFrame(const float* InFrame, float* OutFrame, const float* InKeyFrame)
	{
		const bool bKeyIsInput = InFrame == InKeyFrame;
		if (ProcessKeyFrame(InKeyFrame, OutFrame, bKeyIsInput))
		{
			const int32 NumChannels = GetNumChannels();
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				// Write and read into the look ahead delay line.
				// We apply the compression output of the direct input to the output of this delay line
				// This way sharp transients can be "caught" with the gain.
				float LookaheadOutput = LookaheadDelay[Channel].ProcessAudioSample(InFrame[Channel]);

				// Write into the output with the computed gain value
				OutFrame[Channel] = Gain[Channel] * LookaheadOutput * OutputGain * InputGain;
			}
		}
	}

	void FDynamicsProcessor::ProcessAudioFrame(const float* InFrame, float* OutFrame, const float* InKeyFrame, float* OutGain)
	{
		check(OutGain != nullptr);

		const bool bKeyIsInput = InFrame == InKeyFrame;
		if (ProcessKeyFrame(InKeyFrame, OutFrame, bKeyIsInput))
		{
			const int32 NumChannels = GetNumChannels();
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				// Write and read into the look ahead delay line.
				// We apply the compression output of the direct input to the output of this delay line
				// This way sharp transients can be "caught" with the gain.
				float LookaheadOutput = LookaheadDelay[Channel].ProcessAudioSample(InFrame[Channel]);

				// Write into the output with the computed gain value
				OutFrame[Channel] = Gain[Channel] * LookaheadOutput * OutputGain * InputGain;
				// Also write the output gain value
				OutGain[Channel] = Gain[Channel];
			}
		}
	}

	void FDynamicsProcessor::ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer, const float* InKeyBuffer)
	{
		check(nullptr != InBuffer);
		check(nullptr != OutBuffer);

		const int32 NumChannels = GetNumChannels();
		const int32 KeyNumChannels = GetKeyNumChannels();

		if (InKeyBuffer)
		{
			int32 KeySampleIndex = 0;
			for (int32 SampleIndex = 0; SampleIndex < InNumSamples; SampleIndex += NumChannels)
			{
				const float* KeyFrame = &InKeyBuffer[KeySampleIndex];
				ProcessAudioFrame(&InBuffer[SampleIndex], &OutBuffer[SampleIndex], KeyFrame);
				KeySampleIndex += KeyNumChannels;
			}
		}
		else
		{
			for (int32 SampleIndex = 0; SampleIndex < InNumSamples; SampleIndex += NumChannels)
			{
				const float* KeyFrame = &InBuffer[SampleIndex];
				ProcessAudioFrame(&InBuffer[SampleIndex], &OutBuffer[SampleIndex], KeyFrame);
			}
		}
	}

	void FDynamicsProcessor::ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer, const float* InKeyBuffer, float* OutEnvelope)
	{
		check(nullptr != InBuffer);
		check(nullptr != OutBuffer);
		check(nullptr != OutEnvelope);

		const int32 NumChannels = GetNumChannels();
		const int32 KeyNumChannels = GetKeyNumChannels();

		if (InKeyBuffer)
		{
			int32 KeySampleIndex = 0;
			for (int32 SampleIndex = 0; SampleIndex < InNumSamples; SampleIndex += NumChannels)
			{
				const float* KeyFrame = &InKeyBuffer[KeySampleIndex];
				ProcessAudioFrame(&InBuffer[SampleIndex], &OutBuffer[SampleIndex], KeyFrame, &OutEnvelope[SampleIndex]);
				KeySampleIndex += KeyNumChannels;
			}
		}
		else
		{
			for (int32 SampleIndex = 0; SampleIndex < InNumSamples; SampleIndex += NumChannels)
			{
				const float* KeyFrame = &InBuffer[SampleIndex];
				ProcessAudioFrame(&InBuffer[SampleIndex], &OutBuffer[SampleIndex], KeyFrame, &OutEnvelope[SampleIndex]);
			}
		}
	}

	void FDynamicsProcessor::ProcessAudio(const float* const* const InBuffers, const int32 InNumSamples, float* const* OutBuffers, const float* const* const InKeyBuffers, float* const* OutEnvelopes)
	{
		// NOTE: This is a useful test, but it isn't comprehensive. It can't be sure the passed 
		// in arrays of pointers have a matching number of pointers to the channel counts. It
		// will have to do for now.
		check(nullptr != InBuffers && nullptr != InBuffers[0]);
		check(nullptr != OutBuffers && nullptr != OutBuffers[0]);
		check(nullptr != OutEnvelopes && nullptr != OutEnvelopes[0]);

		const int32 KeyNumChannels = GetKeyNumChannels();
		const int32 NumChannels = GetNumChannels();

		// we need this test because we are going to use the output buffer as our scratch pad for key processing!
		check(KeyNumChannels <= NumChannels);

		const float* const* KeySources = InKeyBuffers ? InKeyBuffers : InBuffers;
		float* const* KeyWorkBuffers = OutBuffers;

		// Process Key
		if (bKeyLowshelfEnabled)
		{
			InputLowshelfFilter.ProcessAudio(KeySources, InNumSamples, KeyWorkBuffers);
			KeySources = KeyWorkBuffers;
		}

		if (bKeyHighshelfEnabled)
		{
			InputHighshelfFilter.ProcessAudio(KeySources, InNumSamples, KeyWorkBuffers);
			KeySources = KeyWorkBuffers;
		}

		const bool bKeyIsInput = !InKeyBuffers || InKeyBuffers == InBuffers;
		float DetectorGain = InputGain;

		// Apply key gain only if detector is key (not input)
		if (!bKeyIsInput)
		{
			DetectorGain *= KeyGain;
		}

		if (bKeyAuditionEnabled)
		{
			if (KeySources != KeyWorkBuffers)
			{
				// This means we have not done either filter above, so we have not  
				// "moved" the key input to the output buffer. We have to do it here. 
				for (int32 Channel = 0; Channel < KeyNumChannels; ++Channel)
				{
					FMemory::Memcpy(OutBuffers[Channel], KeySources[Channel], sizeof(float) * InNumSamples);
				}
			}
			// we just have to zero out any channels that are not in the key signal
			for (int32 Channel = KeyNumChannels; Channel < NumChannels; ++Channel)
			{
				FMemory::Memset(OutBuffers[Channel], 0, sizeof(float) * InNumSamples);
			}

			// I WOULD zero or 1 the "OutEnvelope", but that is not what the original does!
			return;
		}

		for (int32 Channel = 0; Channel < KeyNumChannels; ++Channel)
		{
			// apply detector gain to key
			Audio::ArrayMultiplyByConstant(TArrayView<const float>(KeySources[Channel], InNumSamples), DetectorGain, TArrayView<float>(KeyWorkBuffers[Channel], InNumSamples));
			// EnvelopeFollow key
			EnvFollower[Channel].ProcessBuffer(KeyWorkBuffers[Channel], InNumSamples, OutEnvelopes[Channel]);
		}

		int32 NumGainChannels = 1;
		switch (LinkMode)
		{
		case EDynamicsProcessorChannelLinkMode::Average:
		{
			// average all key channels
			TArrayView<float> Out = TArrayView<float>(OutEnvelopes[0], InNumSamples);
			for (int32 KeyChannel = 1; KeyChannel < KeyNumChannels; ++KeyChannel)
			{
				ArraySum(TArrayView<const float>(OutEnvelopes[KeyChannel], InNumSamples), Out, Out);
			}
			ArrayMultiplyByConstant(Out, 1.0f / (float)KeyNumChannels, Out);
			// convert magnitude to db
			ArrayMagnitudeToDecibelInPlace(Out, -96.0f);
			// compute 1 gain and use for all channels ...	const float ComputedGain = ComputeGain(DetectorOutLinkedDb);
			ComputeGains(OutEnvelopes[0], InNumSamples);
			NumGainChannels = 1;
		}
		break;

		case EDynamicsProcessorChannelLinkMode::Peak:
		{
			// detect peak across all key channels
			TArrayView<float> Out = TArrayView<float>(OutEnvelopes[0], InNumSamples);
			for (int32 KeyChannel = 1; KeyChannel < KeyNumChannels; ++KeyChannel)
			{
				ArrayMax(TArrayView<const float>(OutEnvelopes[KeyChannel], InNumSamples), Out, Out);
			}
			// convert magnitude to db
			ArrayMagnitudeToDecibelInPlace(Out, -96.0f);
			// compute 1 gain and use for all channels ...	const float ComputedGain = ComputeGain(DetectorOutLinkedDb);
			ComputeGains(OutEnvelopes[0], InNumSamples);
			NumGainChannels = 1;
		}
		break;

		case EDynamicsProcessorChannelLinkMode::Disabled:
		default:
		{
			for (int32 Channel = 0; Channel < KeyNumChannels; ++Channel)
			{
				// convert magnitude to db
				ArrayMagnitudeToDecibelInPlace(TArrayView<float>(OutEnvelopes[Channel],InNumSamples), -96.0f);
				// compute 1 gain and use for all channels ...	const float ComputedGain = ComputeGain(DetectorOutLinkedDb);
				ComputeGains(OutEnvelopes[Channel], InNumSamples);
			}
			NumGainChannels = KeyNumChannels;
		}
		break;
		}

		const float OutAndInGain = OutputGain * InputGain;
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			int32 GainChannel = Channel % NumGainChannels;

			// copy input to look ahead delay write position
			LookaheadDelay[Channel].ProcessAudioBuffer(InBuffers[Channel], InNumSamples, OutBuffers[Channel]);
			
			// apply compression gain & output gain & input gain to look ahead delay read position and save to output
			ArrayMultiplyInPlace(TArrayView<const float>(OutEnvelopes[GainChannel], InNumSamples), TArrayView<float>(OutBuffers[Channel], InNumSamples));
			ArrayMultiplyByConstant(TArrayView<const float>(OutBuffers[Channel], InNumSamples), OutAndInGain, TArrayView<float>(OutBuffers[Channel], InNumSamples));
		}
	}

	bool FDynamicsProcessor::ProcessKeyFrame(const float* InKeyFrame, float* OutFrame, bool bKeyIsInput)
	{
		// Get detector outputs
		const float* KeyIn = InKeyFrame;

		const int32 KeyNumChannels = GetKeyNumChannels();
		const int32 NumChannels = GetNumChannels();
		if (KeyNumChannels > 0)
		{
			if (bKeyLowshelfEnabled)
			{
				InputLowshelfFilter.ProcessAudioFrame(KeyIn, DetectorOuts.GetData());
				KeyIn = DetectorOuts.GetData();
			}

			if (bKeyHighshelfEnabled)
			{
				InputHighshelfFilter.ProcessAudioFrame(KeyIn, DetectorOuts.GetData());
				KeyIn = DetectorOuts.GetData();
			}
		}

		float DetectorGain = InputGain;

		// Apply key gain only if detector is key (not input)
		if (!bKeyIsInput)
		{
			DetectorGain *= KeyGain;
		}

		if (bKeyAuditionEnabled)
		{
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				const int32 KeyIndex = Channel % KeyNumChannels;
				OutFrame[Channel] = DetectorGain * KeyIn[KeyIndex];
			}

			return false;
		}

		for (int32 Channel = 0; Channel < KeyNumChannels; ++Channel)
		{
			DetectorOuts[Channel] = EnvFollower[Channel].ProcessSample(DetectorGain * KeyIn[Channel]);
		}

		switch (LinkMode)
		{
			case EDynamicsProcessorChannelLinkMode::Average:
			{
				float KeyOutLinked = 0.0f;
				for (int32 Channel = 0; Channel < KeyNumChannels; ++Channel)
				{
					KeyOutLinked += DetectorOuts[Channel];
				}
				KeyOutLinked /= static_cast<float>(KeyNumChannels);
				const float DetectorOutLinkedDb = ConvertToDecibels(KeyOutLinked);
				const float ComputedGain = ComputeGain(DetectorOutLinkedDb);
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					Gain[Channel] = ComputedGain;
				}
			}
			break;

			case EDynamicsProcessorChannelLinkMode::Peak:
			{
				float KeyOutLinked = FMath::Max<float>(DetectorOuts);
				const float KeyOutLinkedDb = ConvertToDecibels(KeyOutLinked);
				const float ComputedGain = ComputeGain(KeyOutLinkedDb);
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					Gain[Channel] = ComputedGain;
				}
			}
			break;

			case EDynamicsProcessorChannelLinkMode::Disabled:
			default:
			{
				// Compute gain individually per channel and wrap if
				// channel count is greater than key channel count.
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					const int32 KeyIndex = Channel % KeyNumChannels;
					float ChannelGain = DetectorOuts[KeyIndex];

					const float KeyOutDb = ConvertToDecibels(ChannelGain);
					const float ComputedGain = ComputeGain(KeyOutDb);
					Gain[KeyIndex] = ComputedGain;
				}
			}
			break;
		}

		return true;
	}

	bool FDynamicsProcessor::IsInProcessingThreshold(const float InEnvFollowerDb) const
	{
		if (ProcessingMode == EDynamicsProcessingMode::UpwardsCompressor)
		{
			return HalfKneeBandwidthDb >= 0.0f
				&& InEnvFollowerDb < (ThresholdDb - HalfKneeBandwidthDb)
				&& InEnvFollowerDb > (ThresholdDb + HalfKneeBandwidthDb);
		}

		return HalfKneeBandwidthDb >= 0.0f
			&& InEnvFollowerDb > (ThresholdDb - HalfKneeBandwidthDb)
			&& InEnvFollowerDb < (ThresholdDb + HalfKneeBandwidthDb);
	}

	float FDynamicsProcessor::ComputeGain(const float InEnvFollowerDb)
	{
		// If we are in the range of compression
		float SlopeThisSample = SlopeFactor;
		if (IsInProcessingThreshold(InEnvFollowerDb))
		{
			// The knee calculation adjusts the slope to use via lagrangian interpolation through the slope
			float Lagrangian = (InEnvFollowerDb - KneePoints[1].X) / Denominator0Minus1;
			SlopeThisSample = Lagrangian * KneePoints[0].Y;
			Lagrangian = (InEnvFollowerDb - KneePoints[0].X) / Denominator1Minus0;
			SlopeThisSample += Lagrangian * KneePoints[1].Y;

		}

		float OutputGainDb = SlopeThisSample * (ThresholdDb - InEnvFollowerDb);

		if (ProcessingMode == EDynamicsProcessingMode::UpwardsCompressor)
		{
			// if left unchecked Upwards compression will try to apply infinite gain
			OutputGainDb = FMath::Clamp(OutputGainDb, 0.f, UpwardsCompressionMaxGain);
		}
		else
		{
			OutputGainDb = FMath::Min(0.f, OutputGainDb);
		}

		return ConvertToLinear(OutputGainDb);
	}

	
	void FDynamicsProcessor::ComputeGains(float* InEnvFollowerDbOutGain, const int32 InNumSamples)
	{
		for (int32 SampleIndex = 0; SampleIndex < InNumSamples; ++SampleIndex)
		{
			float InEnvFollowerDb = InEnvFollowerDbOutGain[SampleIndex];

			float SlopeThisSample = SlopeFactor;
			if (IsInProcessingThreshold(InEnvFollowerDb))
			{
				float Lagrangian = (InEnvFollowerDb - KneePoints[1].X) / Denominator0Minus1;
				SlopeThisSample = Lagrangian * KneePoints[0].Y;
				Lagrangian = (InEnvFollowerDb - KneePoints[0].X) / Denominator1Minus0;
				SlopeThisSample += Lagrangian * KneePoints[1].Y;
			};

			float OutputGainDb = SlopeThisSample * (ThresholdDb - InEnvFollowerDb);

			if (ProcessingMode == EDynamicsProcessingMode::UpwardsCompressor)
			{
				// if left unchecked Upwards compression will try to apply infinite gain
				OutputGainDb = FMath::Clamp(OutputGainDb, 0.f, UpwardsCompressionMaxGain);
				InEnvFollowerDbOutGain[SampleIndex] = ConvertToLinear(OutputGainDb);
			}
			else
			{
				InEnvFollowerDbOutGain[SampleIndex] = OutputGainDb > -UE_SMALL_NUMBER ? 1.0f : ConvertToLinear(OutputGainDb);
			}
		}
	}

	void FDynamicsProcessor::CalculateSlope()
	{
		SlopeFactor = 0.0f;
		// Depending on the mode, we define the "slope". 
		switch (ProcessingMode)
		{
		default:

			// Compressors smoothly reduce the gain as the gain gets louder
			// CompressionRatio -> Inifinity is a limiter
			// Upwards compression applies gain when below a threshold, but uses the same slope
		case EDynamicsProcessingMode::UpwardsCompressor:
		case EDynamicsProcessingMode::Compressor:
			SlopeFactor = 1.0f - 1.0f / Ratio;
			break;

			// Limiters do nothing until it hits the threshold then clamps the output hard
		case EDynamicsProcessingMode::Limiter:
			SlopeFactor = 1.0f;
			break;

			// Expanders smoothly increase the gain as the gain gets louder
			// CompressionRatio -> Infinity is a gate
		case EDynamicsProcessingMode::Expander:
			SlopeFactor = 1.0f / Ratio - 1.0f;
			break;

			// Gates are opposite of limiter. They stop sound (stop gain) until the threshold is hit
		case EDynamicsProcessingMode::Gate:
			SlopeFactor = -1.0f;
			break;
		}
		CalculateKnee();
	}

	void FDynamicsProcessor::CalculateKnee()
	{
		// Setup the knee for interpolation. Don't allow the top knee point to exceed 0.0
		KneePoints[0].X = ThresholdDb - HalfKneeBandwidthDb;
		KneePoints[1].X = FMath::Min(ThresholdDb + HalfKneeBandwidthDb, 0.0f);

		KneePoints[0].Y = 0.0f;
		KneePoints[1].Y = SlopeFactor;

		// These next few calculations let us optimize out the call to 
		// the LagrangeInterpolation that used to be in there. We precalculate 
		// some coefficients that remain the same unless the KneePoints change.
		Denominator0Minus1 = KneePoints[0].X - KneePoints[1].X;
		if (FMath::Abs(Denominator0Minus1) < UE_SMALL_NUMBER)
		{
			Denominator0Minus1 = UE_SMALL_NUMBER;
		}
		Denominator1Minus0 = KneePoints[1].X - KneePoints[0].X;
		if (FMath::Abs(Denominator1Minus0) < UE_SMALL_NUMBER)
		{
			Denominator1Minus0 = UE_SMALL_NUMBER;
		}
	}
}
