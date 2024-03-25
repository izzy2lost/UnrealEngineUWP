// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiStream.h"

#include "MetasoundDataTypeRegistrationMacro.h"
#include "Algo/BinarySearch.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

REGISTER_METASOUND_DATATYPE(HarmonixMetasound::FMidiStream, "MIDIStream")

namespace HarmonixMetasound
{
	DEFINE_LOG_CATEGORY(LogMidiStreamDataType);

	using namespace Metasound;
	
	FMidiStream::FMidiStream(const FOperatorSettings&)
	{
	}

	void FMidiStream::SetClock(const FMidiClock& InClock)
	{
		Clock = InClock.AsWeak();
	}

	void FMidiStream::ResetClock()
	{
		Clock.Reset();
	}

	TSharedPtr<const FMidiClock, ESPMode::NotThreadSafe> FMidiStream::GetClock() const
	{
		return Clock.Pin();
	}


	void FMidiStream::PrepareBlock()
	{
		EventsInBlock.Empty(32);
	}

	void FMidiStream::AddMidiEvent(const FMidiStreamEvent& Event)
	{
		check (EventsInBlock.IsEmpty() || EventsInBlock.Last().BlockSampleFrameIndex <= Event.BlockSampleFrameIndex);
		EventsInBlock.Add(Event);

		UpdateActiveVoice(Event);
	}

	void FMidiStream::InsertMidiEvent(const FMidiStreamEvent& Event)
	{
		int32 AtIndex = Algo::UpperBound(EventsInBlock, Event, [](const FMidiStreamEvent& NewEvent, const FMidiStreamEvent& ExistingEvent){ return NewEvent.BlockSampleFrameIndex < ExistingEvent.BlockSampleFrameIndex; });
		EventsInBlock.Insert(Event, AtIndex);

		UpdateActiveVoice(Event);
	}

	void FMidiStream::AddNoteOffEventOrCancelPendingNoteOn(const FMidiStreamEvent& Event)
	{
		check(Event.MidiMessage.IsNoteOff());
		int32 NumRemoved = EventsInBlock.RemoveAll([&](const FMidiStreamEvent& EventInList)
			{
				return EventInList.MidiMessage.IsNoteOn() && EventInList.GetVoiceId() == Event.GetVoiceId();
			});
		if (NumRemoved == 0)
		{
			AddMidiEvent(Event);
		}
	}

	void FMidiStream::InsertNoteOffEventOrCancelPendingNoteOn(const FMidiStreamEvent& Event)
	{
		check(Event.MidiMessage.IsNoteOff());
		int32 NumRemoved = EventsInBlock.RemoveAll([&](const FMidiStreamEvent& EventInList)
			{
				return EventInList.MidiMessage.IsNoteOn() && EventInList.GetVoiceId() == Event.GetVoiceId();
			});
		if (NumRemoved == 0)
		{
			InsertMidiEvent(Event);
		}
	}

	const FString* FMidiStream::GetMidiTrackText(int32 TrackNumber, int32 TextIndex) const
	{
		if (!MidiFileSourceOfEvents || TrackNumber < 0 || TextIndex < 0)
		{
			return nullptr;
		}

		TSharedPtr<FMidiFileData> MidiFile = MidiFileSourceOfEvents->GetMidiFile();
		if (TrackNumber >= MidiFile->Tracks.Num())
		{
			return nullptr;
		}
		const FMidiTextRepository* Repository = MidiFile->Tracks[TrackNumber].GetTextRepository();
		if (TextIndex >= Repository->Num())
		{
			return nullptr;
		}
		return &(*Repository)[TextIndex];
	}

	void FMidiStream::Copy(const FMidiStream& From, FMidiStream& To, const FEventFilter& Filter, const FEventTransformer& Transformer)
	{
		// Copy the clock from the other stream
		if (const TSharedPtr<const FMidiClock, ESPMode::NotThreadSafe> FromClock = From.GetClock())
		{
			To.SetClock(*FromClock);
		}

		// Reset the target
		To.EventsInBlock.Reset();
		To.ActiveVoices.Reset();
		
		// Copy the events
		for (const FMidiStreamEvent& Event : From.GetEventsInBlock())
		{
			if (Filter(Event))
			{
				To.AddMidiEvent(Transformer(Event));
			}
		}
	}

	void FMidiStream::Merge(const FMidiStream& From, FMidiStream& To, const FEventFilter& Filter, const FEventTransformer& Transformer)
	{
		// We need to make sure that the clocks match, or that one of them doesn't have a clock.
		// Otherwise a merge is invalid.
		{
			const auto FromClock = From.GetClock();
			const auto ToClock = To.GetClock();
			
			if (FromClock.Get() != ToClock.Get() && FromClock.IsValid() && ToClock.IsValid())
			{
				return;
			}

			// If the "to" clock is null, and the "from" clock isn't, overwrite the "to" clock
			if (FromClock.IsValid() && !ToClock.IsValid())
			{
				To.SetClock(*FromClock);
			}
		}

		// Insert the events
		for (const FMidiStreamEvent& Event : From.GetEventsInBlock())
		{
			if (Filter(Event))
			{
				To.InsertMidiEvent(Transformer(Event));
			}
		}
	}
	
	void FMidiStream::Merge(
		const FMidiStream& FromA,
		const FMidiStream& FromB,
		FMidiStream& To,
		const FEventFilter& Filter,
		const FEventTransformer& Transformer)
	{
		// Merge in stream A
		Merge(FromA, To, Filter, Transformer);

		// Merge in stream B and re-map the voice ids
		const auto RemapTransform = [&To, &Transformer](const FMidiStreamEvent& Event)
		{
			FMidiStreamEvent TransformedEvent = Transformer(Event);
			const auto GeneratorId = TransformedEvent.GetVoiceId().GetGeneratorId();
			TransformedEvent.ReassignOwner(&To.GeneratorMap.FindOrAdd(GeneratorId));
			return TransformedEvent;
		};
		Merge(FromB, To, Filter, RemapTransform);
	}

	void FMidiStream::UpdateActiveVoice(const FMidiStreamEvent& Event)
	{
		if (Event.MidiMessage.IsNoteMessage())
		{
			if (Event.MidiMessage.IsNoteOn())
			{
				if (FMidiVoiceId VoiceId = Event.GetVoiceId(); !ActiveVoices.Contains(VoiceId))
				{
					ActiveVoices.Emplace(MoveTemp(VoiceId));
				}
			}
			else if (Event.MidiMessage.IsNoteOff())
			{
				ActiveVoices.Remove(Event.GetVoiceId());
			}
			// Kill all or all off
			else
			{
				ActiveVoices.Reset();
			}
		}
	}

	void FMidiVoiceTracker::Process(const FMidiStream& MidiStream, const FKillVoiceFn& KillVoiceFn)
	{
		// Remove active voices that have note offs this block
		for (const auto& MidiEvent : MidiStream.EventsInBlock)
		{
			if (MidiEvent.MidiMessage.IsNoteOff())
			{
				ActiveVoices.Remove(MidiEvent.GetVoiceId());
			}
		}

		// Check for voices that are no longer active and notify the caller
		for (auto It = ActiveVoices.CreateIterator(); It; ++It)
		{
			if (!MidiStream.ActiveVoices.Contains(*It))
			{
				KillVoiceFn(*It);
			}
		}

		ActiveVoices = MidiStream.ActiveVoices;
	}

}
