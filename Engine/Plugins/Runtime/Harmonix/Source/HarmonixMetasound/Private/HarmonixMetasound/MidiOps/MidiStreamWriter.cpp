// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/MidiOps/MidiStreamWriter.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMidi/MidiWriter.h"

namespace Harmonix::Midi::Ops
{
	using namespace HarmonixMetasound;
	FMidiStreamWriter::FMidiStreamWriter(TUniquePtr<FArchive>&& InArchive)
		: Archive(MoveTemp(InArchive))
	{}

	void FMidiStreamWriter::Process(const FMidiStream& InStream)
	{
		ensureMsgf(InStream.GetClock(), TEXT("Midi stream must have a midi clock for the MidiStreamWriter to process it"));
		if (TSharedPtr<const FMidiClock, ESPMode::NotThreadSafe> MidiClock = InStream.GetClock())
		{
			for (const FMidiClockEvent& ClockEvent : MidiClock->GetMidiClockEventsInBlock())
			{
				if (ClockEvent.Msg.IsType<MidiClockMessageTypes::FAdvanceThru>())
				{
					const MidiClockMessageTypes::FAdvanceThru& AdvanceThru = ClockEvent.Msg.Get<MidiClockMessageTypes::FAdvanceThru>();
					
					if (!AdvanceThru.IsPreRoll)
					{
						Process(InStream, AdvanceThru.FromTick, AdvanceThru.ThruTick);
					}
				}
			}
		}
	}


	void FMidiStreamWriter::Process(const FMidiStream& InStream, int32 FromTick, int32 ThruTick)
	{
		bool AddedEvents = false;
		for (const FMidiStreamEvent& MidiEvent : InStream.GetEventsInBlock())
		{
			if (MidiEvent.CurrentMidiTick > FromTick && MidiEvent.CurrentMidiTick <= ThruTick)
			{
				const int32 OffsetTick = MidiEvent.CurrentMidiTick - FromTick;
				const int32 MidiTick = CurrentWriteTick + OffsetTick;
				FMidiTrack& MidiTrack = MidiTracks.FindOrAdd(MidiEvent.TrackIndex);
				MidiTrack.AddEvent(FMidiEvent(MidiTick, MidiEvent.MidiMessage));
				AddedEvents = true;
			}
		}
		CurrentWriteTick += ThruTick - FromTick;

		if (AddedEvents)
		{
			Archive->Seek(0);
			FMidiWriter MidiWriter = FMidiWriter(*Archive);
			for (TPair<int32, const FMidiTrack&> Pair : MidiTracks)
			{
				Pair.Value.WriteStdMidi(MidiWriter);
			}
		}
	}
};