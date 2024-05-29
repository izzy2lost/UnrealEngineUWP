// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/MidiOps/PulseGenerator.h"

namespace Harmonix::Midi::Ops
{
	void FPulseGenerator::Enable(bool bEnable)
	{
		Enabled = bEnable;
	}

	void FPulseGenerator::SetClock(const TSharedPtr<const HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe>& NewClock)
	{
		Clock = NewClock;
	}

	void FPulseGenerator::SetInterval(const FMusicTimeInterval& NewInterval)
	{
		Interval = NewInterval;

		// Multiplier should be >= 1
		Interval.IntervalMultiplier = FMath::Max(Interval.IntervalMultiplier, static_cast<uint16>(1));
	}

	void FPulseGenerator::Process(HarmonixMetasound::FMidiStream& OutStream)
	{
		using namespace HarmonixMetasound;
		using namespace HarmonixMetasound::MidiClockMessageTypes;

		OutStream.PrepareBlock();
		
		const auto PinnedClock = Clock.Pin();

		if (!PinnedClock)
		{
			return;
		}

		for (const FMidiClockEvent& ClockEvent : PinnedClock->GetMidiClockEventsInBlock())
		{
			if (const FAdvance* AsAdvance = ClockEvent.TryGet<FAdvance>())
			{
				HandleAdvance(ClockEvent.BlockFrameIndex, AsAdvance, OutStream);
			}
			else if (const FTimeSignatureChange* AsTimeSigChange = ClockEvent.TryGet<FTimeSignatureChange>())
			{
				HandleTimeSignatureChange(AsTimeSigChange);
			}
		}
	}

	void FPulseGenerator::HandleAdvance(const int32 BlockFrameIndex, const HarmonixMetasound::MidiClockMessageTypes::FAdvance* Advance, HarmonixMetasound::FMidiStream& OutStream)
	{
		if (!NextPulseTimestamp.IsValid())
		{
			return;
		}

		check(Clock.IsValid()); // if we're here and we don't have a clock, we have problems
		const auto PinnedClock = Clock.Pin();
		int32 NextPulseTick = PinnedClock->GetSongMapEvaluator().MusicTimestampToTick(NextPulseTimestamp);

		while (Advance->LastTickToProcess() >= NextPulseTick)
		{
			DoPulse(BlockFrameIndex, NextPulseTick, OutStream);

			IncrementTimestampByInterval(NextPulseTimestamp, Interval, CurrentTimeSignature);

			NextPulseTick = PinnedClock->GetSongMapEvaluator().MusicTimestampToTick(NextPulseTimestamp);
		}
	}

	void FPulseGenerator::HandleTimeSignatureChange(const HarmonixMetasound::MidiClockMessageTypes::FTimeSignatureChange* TimeSigChange)
	{
		CurrentTimeSignature = TimeSigChange->TimeSignature;

		check(Clock.IsValid()); // if we're here and we don't have a clock, we have problems
		const auto PinnedClock = Clock.Pin();
		// Time sig changes will come on the downbeat, and if we change time signature,
		// we want to reset the pulse, so the next pulse is now plus the offset
		NextPulseTimestamp = PinnedClock->GetSongMapEvaluator().TickToMusicTimestamp(TimeSigChange->Tick);
		IncrementTimestampByOffset(NextPulseTimestamp, Interval, CurrentTimeSignature);
	}

	void FPulseGenerator::DoPulse(const int32 BlockFrameIndex, const int32 PulseTick, HarmonixMetasound::FMidiStream& OutStream)
	{
		int32 NoteOnSample = BlockFrameIndex;

		// Note off if there was a previous note on
		if (LastNoteOn.IsSet())
		{
			check(LastNoteOn->MidiMessage.IsNoteOn());

			// Trigger the note off one sample before the note on
			const int32 NoteOffSample = BlockFrameIndex > 0 ? BlockFrameIndex - 1 : BlockFrameIndex;
			NoteOnSample = NoteOffSample + 1;

			// Trigger the note off one tick before the note on
			const int32 NoteOffTick = PulseTick - 1;

			FMidiMsg Msg{ FMidiMsg::CreateNoteOff(LastNoteOn->MidiMessage.GetStdChannel(), LastNoteOn->MidiMessage.GetStdData1()) };
			HarmonixMetasound::FMidiStreamEvent Event{ &VoiceGenerator, Msg };
			Event.BlockSampleFrameIndex = NoteOffSample;
			Event.AuthoredMidiTick = NoteOffTick;
			Event.CurrentMidiTick = NoteOffTick;
			Event.TrackIndex = LastNoteOn->TrackIndex;
			OutStream.InsertMidiEvent(Event);

			LastNoteOn.Reset();
		}

		// Note on
		if (Enabled)
		{
			FMidiMsg Msg{ FMidiMsg::CreateNoteOn(Channel - 1, NoteNumber, Velocity) };
			HarmonixMetasound::FMidiStreamEvent Event{ &VoiceGenerator, Msg };
			Event.BlockSampleFrameIndex = NoteOnSample;
			Event.AuthoredMidiTick = PulseTick;
			Event.CurrentMidiTick = PulseTick;
			Event.TrackIndex = Track;
			OutStream.InsertMidiEvent(Event);

			LastNoteOn.Emplace(MoveTemp(Event));
		}
	}
}
