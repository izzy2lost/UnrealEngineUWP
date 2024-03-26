// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/MidiOps/PulseGenerator.h"

namespace Harmonix::Midi::Ops
{
	void FPulseGenerator::Enable(bool bEnable)
	{
		Enabled = bEnable;
	}

	void FPulseGenerator::SetClock(const HarmonixMetasound::FMidiClock* Clock)
	{
		Cursor.SetClock(Clock);
	}

	void FPulseGenerator::SetInterval(const FMusicTimeInterval& NewInterval)
	{
		Cursor.SetInterval(NewInterval);
	}

	void FPulseGenerator::Process(HarmonixMetasound::FMidiStream& OutStream)
	{
		OutStream.PrepareBlock();
		
		// Keep draining the queue if disabled, so we get the next note off,
		// and so we stay in phase if we toggle off and back on
		FCursor::FPulseTime NextPulse;
		while (Cursor.Pop(NextPulse))
		{
			int32 NoteOnSample = NextPulse.AudioBlockFrame;

			// Note off if there was a previous note on
			if (LastNoteOn.IsSet())
			{
				check(LastNoteOn->MidiMessage.IsNoteOn());
				
				// Trigger the note off one sample before the note on
				const int32 NoteOffSample = NextPulse.AudioBlockFrame > 0 ? NextPulse.AudioBlockFrame - 1 : NextPulse.AudioBlockFrame;
				NoteOnSample = NoteOffSample + 1;

				// Trigger the note off one tick before the note on
				const int32 NoteOffTick = NextPulse.MidiTick - 1;

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
				Event.AuthoredMidiTick = NextPulse.MidiTick;
				Event.CurrentMidiTick = NextPulse.MidiTick;
				Event.TrackIndex = Track;
				OutStream.InsertMidiEvent(Event);

				LastNoteOn.Emplace(MoveTemp(Event));
			}
		}
	}

	FPulseGenerator::FCursor::FCursor()
	{
		SetMessageFilter(EFilterPassFlags::TimeSig);
	}

	void FPulseGenerator::FCursor::SetClock(const HarmonixMetasound::FMidiClock* NewClock)
	{
		if (Clock == NewClock)
		{
			return;
		}
		
		if (nullptr == NewClock)
		{
			if (nullptr != Owner)
			{
				Owner->UnregisterPlayCursor(this);
			}
		}
		else
		{
			NewClock->RegisterHiResPlayCursor(this);
		}

		Clock = NewClock;
	}

	void FPulseGenerator::FCursor::SetInterval(const FMusicTimeInterval& NewInterval)
	{
		Interval = NewInterval;

		// Multiplier should be >= 1
		Interval.IntervalMultiplier = FMath::Max(Interval.IntervalMultiplier, static_cast<uint16>(1));
	}

	bool FPulseGenerator::FCursor::Pop(FPulseTime& PulseTime)
	{
		if (PulsesSinceLastProcess.Dequeue(PulseTime))
		{
			--QueueSize;
			return true;
		}

		return false;
	}

	void FPulseGenerator::FCursor::Push(FPulseTime&& PulseTime)
	{
		constexpr int32 MaxQueueSize = 1024; // just to keep the queue from infinitely growing if we stop draining it
		if (QueueSize < MaxQueueSize)
		{
			PulsesSinceLastProcess.Enqueue(MoveTemp(PulseTime));
			++QueueSize;
		}
	}

	void FPulseGenerator::FCursor::AdvanceThruTick(int32 Tick, bool IsPreRoll)
	{
		FMidiPlayCursor::AdvanceThruTick(Tick, IsPreRoll);

		if (!NextPulseTimestamp.IsValid())
		{
			return;
		}

		check(nullptr != Clock);
		int32 NextPulseTick = Clock->GetBarMap().MusicTimestampToTick(NextPulseTimestamp);

		while (CurrentTick >= NextPulseTick)
		{
			Push({ Clock->GetCurrentBlockFrameIndex(), NextPulseTick });
			
			IncrementTimestampByInterval(NextPulseTimestamp, Interval, CurrentTimeSignature);

			NextPulseTick = Clock->GetBarMap().MusicTimestampToTick(NextPulseTimestamp);
		}
	}

	void FPulseGenerator::FCursor::OnTimeSig(int32 TrackIndex, int32 Tick, int32 Numerator, int32 Denominator, bool IsPreroll)
	{
		CurrentTimeSignature.Numerator = Numerator;
		CurrentTimeSignature.Denominator = Denominator;
		
		check(nullptr != Owner);
		// Time sig changes will come on the downbeat, and if we change time signature,
		// we want to reset the pulse, so the next pulse is now plus the offset
		NextPulseTimestamp = Owner->GetBarMap().TickToMusicTimestamp(Tick);
		IncrementTimestampByOffset(NextPulseTimestamp, Interval, CurrentTimeSignature);
	}
}
