// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/SpscQueue.h"

#include "HarmonixDsp/Parameters/Parameter.h"

#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MusicTimeInterval.h"

#include "HarmonixMidi/MidiVoiceId.h"

namespace Harmonix::Midi::Ops
{
	class HARMONIXMETASOUND_API FPulseGenerator
	{
	public:
		void Enable(bool bEnable);
		
		void SetClock(const TSharedPtr<const HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe>& Clock);

		Dsp::Parameters::TParameter<uint8> Channel{ 1, 16, 1 };

		Dsp::Parameters::TParameter<uint16> Track{ 1, UINT16_MAX, 1 };
		
		Dsp::Parameters::TParameter<uint8> NoteNumber{ 0, 127, 60 };
		
		Dsp::Parameters::TParameter<uint8> Velocity{ 0, 127, 127 };
		
		void SetInterval(const FMusicTimeInterval& NewInterval);

		FMusicTimeInterval GetInterval() const { return Interval; }

		void Process(HarmonixMetasound::FMidiStream& OutStream);

	private:
		void HandleAdvance(const int32 BlockFrameIndex, const HarmonixMetasound::MidiClockMessageTypes::FAdvance* Advance, HarmonixMetasound::FMidiStream& OutStream);
		void HandleTimeSignatureChange(const HarmonixMetasound::MidiClockMessageTypes::FTimeSignatureChange* TimeSigChange);
		void DoPulse(const int32 BlockFrameIndex, const int32 PulseTick, HarmonixMetasound::FMidiStream& OutStream);

		FMidiVoiceGeneratorBase VoiceGenerator{};
		TOptional<HarmonixMetasound::FMidiStreamEvent> LastNoteOn;
		bool Enabled{ true };
		TWeakPtr<const HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe> Clock;
		FMusicTimeInterval Interval{};
		FTimeSignature CurrentTimeSignature{};
		FMusicTimestamp NextPulseTimestamp{ -1, -1 };
	};
}
