// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "Algo/BinarySearch.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

#include "MetasoundDataTypeRegistrationMacro.h"

REGISTER_METASOUND_DATATYPE(HarmonixMetasound::FMidiStream, "MidiStream")

namespace HarmonixMetasound
{
	DEFINE_LOG_CATEGORY(LogMidiStreamDataType);

	using namespace Metasound;

	FMidiStream::FMidiStream(const FOperatorSettings& InSettings)
		: NumFramesPerBlock(InSettings.GetNumFramesPerBlock())
		, SampleRate(InSettings.GetSampleRate())
	{

	}

	void FMidiStream::PrepareBlock()
	{
		TransportChangesInBlock.Empty(4);
		EventsInBlock.Empty(32);
	}

	void FMidiStream::AddTransportStateChangeMessage(int32 SampleFrameIndexInBlock, EMusicPlayerTransportState State)
	{
		if (State == CurrentTransportState.TransportState)
		{
			return;
		}
		CurrentTransportState.BlockSampleFrameIndex = SampleFrameIndexInBlock;
		CurrentTransportState.BlockSampleFrameIndex = 0.0f;
		CurrentTransportState.TransportState        = State;
		TransportChangesInBlock.Add(CurrentTransportState);
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

	void FMidiStream::Copy(const TArray<FMidiStreamReadRef>& InStreams, bool EventsOnly)
	{
		if (!InStreams.Num())
		{
			return;
		}

		if (InStreams.Num() == 1)
		{
			Copy(InStreams[0]);
			return;
		}

		TArray<FMidiStreamReadRef> SameClockStreams;
		FilterArrayToStreamsWithTheSameClock(InStreams, SameClockStreams);
		if (!EventsOnly)
		{
			CopyTransportEvents(SameClockStreams);
		}
		CopyMidiEvents(SameClockStreams);
	}

	void FMidiStream::Copy(FMidiStreamReadRef InStream, bool EventsOnly)
	{
		if (GetMidiClockSource() && GetMidiClockSource()->Get() != InStream->GetMidiClockSource()->Get())
		{
			UE_LOG(LogMidiStreamDataType, Warning, TEXT("Cannot copy midi events from one stream to another if they have different clocks."));
			return;
		}
		if (EventsOnly)
		{
			CopyTransportEvents(InStream);
		}
		CopyMidiEvents(InStream);
	}

	void FMidiStream::FilterArrayToStreamsWithTheSameClock(const TArray<FMidiStreamReadRef>& InStreams, TArray<FMidiStreamReadRef>& OutStreams)
	{
		// This has no clock so pass all events
		if (!GetMidiClockSource())
		{
			OutStreams.Append(InStreams);
			return;
		}

		for (const FMidiStreamReadRef& Stream : InStreams)
		{
			// Allow the copy if the stream matches or the stream has no clock
			// Streams with no clock are ok because they won't conflict with our clock
			// And chances are, that stream has no events
			if (!Stream->GetMidiClockSource() || Stream->GetMidiClockSource()->Get() == GetMidiClockSource()->Get())
			{
				OutStreams.Add(Stream);
			}
			else
			{
				UE_LOG(LogMidiStreamDataType, Warning, TEXT("Cannot copy midi events from one stream to another if they have different clocks."));
			}
		}
	}

	void FMidiStream::CopyTransportEvents(const TArray<FMidiStreamReadRef>& InStreams)
	{
		TransportChangesInBlock.Empty(4);

		using TransportIterator = TArray<FMidiTimestampTransportState>::RangedForConstIteratorType;

		int32 NumInStreams = InStreams.Num();
		TArray<TransportIterator> WalkingIterators;
		WalkingIterators.Reserve(NumInStreams);
		TArray<TransportIterator> IteratorEnds;
		IteratorEnds.Reserve(NumInStreams);

		for (int32 i = 0; i < NumInStreams; ++i)
		{
			WalkingIterators.Add(InStreams[i]->GetTransportChangesInBlock().begin());
			IteratorEnds.Add(InStreams[i]->GetTransportChangesInBlock().end());
		}

		while(1)
		{
			int32 StreamWithEarliestEvent = -1;
			int32 EarliestBlockSampleFrameIndex = MAX_int32;

			for (int32 IteratorIndex = 0; IteratorIndex < NumInStreams; ++IteratorIndex)
			{
				if (WalkingIterators[IteratorIndex] != IteratorEnds[IteratorIndex])
				{
					if ((*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex < EarliestBlockSampleFrameIndex)
					{
						EarliestBlockSampleFrameIndex = (*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex;
						StreamWithEarliestEvent = IteratorIndex;
					}
				}
			}

			// done?
			if (StreamWithEarliestEvent == -1)
			{
				break;
			}

			AddTransportStateChangeMessage((*WalkingIterators[StreamWithEarliestEvent]).BlockSampleFrameIndex, (*WalkingIterators[StreamWithEarliestEvent]).TransportState);

			++WalkingIterators[StreamWithEarliestEvent];
		}
	}

	void FMidiStream::CopyTransportEvents(const FMidiStreamReadRef& InStream)
	{
		const TArray<FMidiTimestampTransportState>& TransportChanges = InStream->GetTransportChangesInBlock();
		TransportChangesInBlock.Empty(TransportChanges.Num());
		for (auto& Change : TransportChanges)
		{
			AddTransportStateChangeMessage(Change.BlockSampleFrameIndex, Change.TransportState);
		}
	}

	void FMidiStream::CopyMidiEvents(const TArray<FMidiStreamReadRef>& InStreams)
	{
		using MidiStreamIterator = TArray<FMidiStreamEvent>::RangedForConstIteratorType;
		
		int32 NumInStreams = InStreams.Num();
		TArray<MidiStreamIterator> WalkingIterators;
		WalkingIterators.Reserve(NumInStreams);
		TArray<MidiStreamIterator> IteratorEnds;
		IteratorEnds.Reserve(NumInStreams);

		int32 NumTotalEvents = 0;
		for (int32 i = 0; i < NumInStreams; ++i)
		{
			WalkingIterators.Add(InStreams[i]->GetEventsInBlock().begin());
			IteratorEnds.Add(InStreams[i]->GetEventsInBlock().end());
			NumTotalEvents += InStreams[i]->GetEventsInBlock().Num();

			// Ensure that the active voices from the source streams are in this stream
			ActiveVoices.Append(InStreams[i]->ActiveVoices);
		}

		EventsInBlock.Empty(FMath::Max(32, NumTotalEvents));

		while (1)
		{
			int32 StreamWithEarliestEvent = -1;
			int32 EarliestBlockSampleIndex = MAX_int32;

			for (int32 IteratorIndex = 0; IteratorIndex < NumInStreams; ++IteratorIndex)
			{
				if (WalkingIterators[IteratorIndex] != IteratorEnds[IteratorIndex])
				{
					if ((*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex == EarliestBlockSampleIndex)
					{
						if ((*WalkingIterators[StreamWithEarliestEvent]).MidiMessage.IsNoteOn() && (*WalkingIterators[IteratorIndex]).MidiMessage.IsNoteOff())
						{
							StreamWithEarliestEvent = IteratorIndex;
						}
					}
					else if ((*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex < EarliestBlockSampleIndex)
					{
						EarliestBlockSampleIndex = (*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex;
						StreamWithEarliestEvent = IteratorIndex;
					}
				}
			}

			// done?
			if (StreamWithEarliestEvent == -1)
			{
				break;
			}

			AddMidiEvent(*WalkingIterators[StreamWithEarliestEvent]);
			++WalkingIterators[StreamWithEarliestEvent];
		}
	}

	void FMidiStream::MergeMidiEvents(const TArray<FMidiStreamReadRef>& InStreams)
	{
		using MidiStreamIterator = TArray<FMidiStreamEvent>::RangedForConstIteratorType;

		int32 NumInStreams = InStreams.Num();
		TArray<MidiStreamIterator> WalkingIterators;
		WalkingIterators.Reserve(NumInStreams);
		TArray<MidiStreamIterator> IteratorEnds;
		IteratorEnds.Reserve(NumInStreams);

		int32 NumTotalEvents = 0;
		for (int32 i = 0; i < NumInStreams; ++i)
		{
			WalkingIterators.Add(InStreams[i]->GetEventsInBlock().begin());
			IteratorEnds.Add(InStreams[i]->GetEventsInBlock().end());
			NumTotalEvents += InStreams[i]->GetEventsInBlock().Num();
		}

		EventsInBlock.Reserve(FMath::Max(32, NumTotalEvents + EventsInBlock.Num()));

		while (1)
		{
			int32 StreamWithEarliestEvent = -1;
			int32 EarliestBlockSampleIndex = MAX_int32;

			for (int32 IteratorIndex = 0; IteratorIndex < NumInStreams; ++IteratorIndex)
			{
				if (WalkingIterators[IteratorIndex] != IteratorEnds[IteratorIndex])
				{
					if ((*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex < EarliestBlockSampleIndex)
					{
						EarliestBlockSampleIndex = (*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex;
						StreamWithEarliestEvent = IteratorIndex;
					}
				}
			}

			// done?
			if (StreamWithEarliestEvent == -1)
			{
				break;
			}

			InsertMidiEvent(*WalkingIterators[StreamWithEarliestEvent]);
			++WalkingIterators[StreamWithEarliestEvent];
		}
	}

	void FMidiStream::CopyMidiEvents(const FMidiStreamReadRef& InStream)
	{
		const TArray<FMidiStreamEvent>& MidiEvents = InStream->GetEventsInBlock();
		EventsInBlock.Empty(FMath::Max(MidiEvents.Num(), 32));
		// Ensure that the active voices from the source match this stream
		ActiveVoices = InStream->ActiveVoices;
		for (auto& Event : MidiEvents)
		{
			AddMidiEvent(Event);
		}
	}

	void FMidiStream::MergeMidiEvents(const FMidiStreamReadRef& InStream)
	{
		const TArray<FMidiStreamEvent>& NewMidiEvents = InStream->GetEventsInBlock();
		EventsInBlock.Reserve(FMath::Max(32, EventsInBlock.Num() + NewMidiEvents.Num()));
		for (auto& Event : NewMidiEvents)
		{
			InsertMidiEvent(Event);
		}
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

	uint64 FMidiStreamEventTrackChannelFilter::StringToBitfield(const FString& InString, FString& OutErrorMessage)
	{
		OutErrorMessage.Empty();
		uint64 Bitfield = 0;
		auto SetBit = [&](uint32 bit) { Bitfield |= (uint64(1) << bit); };

		if (InString.Equals(TEXT("*")))
		{
			Bitfield = std::numeric_limits<uint64>::max();
			return Bitfield;
		}

		TArray<FString> CommaParts;
		InString.ParseIntoArray(CommaParts, TEXT(","));
		for (const FString& CommaPart : CommaParts)
		{
			TArray<FString> DashParts;
			CommaPart.ParseIntoArray(DashParts, TEXT("-"));
			if (DashParts.IsEmpty() || DashParts.Num() > 2)
			{
				OutErrorMessage = FString::Printf(TEXT("Unexpected filter expression -> \"%s\""), *CommaPart);
				return 0;
			}

			if (DashParts.Num() == 1)
			{
				int32 AsNumber;
				if (!LexTryParseString(AsNumber, *DashParts[0]))
				{
					OutErrorMessage = FString::Printf(TEXT("Error parsing part of filter string -> \"%s\""), *DashParts[0]);
					return 0;
				}

				if (AsNumber <= 0)
				{
					OutErrorMessage = TEXT("Track and Channel filter indexes should be '1' based!");
					return 0;
				}

				SetBit(AsNumber - 1);
			}
			else if (DashParts.Num() == 2)
			{
				int32 AsNumberStart;
				int32 AsNumberEnd;
				if (!LexTryParseString(AsNumberStart, *DashParts[0]))
				{
					OutErrorMessage = FString::Printf(TEXT("Error parsing part of filter string -> \"%s\""), *DashParts[0]);
					return 0;
				}

				if (!LexTryParseString(AsNumberEnd, *DashParts[1]))
				{
					OutErrorMessage = FString::Printf(TEXT("Error parsing part of filter string -> \"%s\""), *DashParts[1]);
					return 0;
				}

				if (AsNumberStart <= 0)
				{
					OutErrorMessage = TEXT("Track and Channel filter indexes should be '1' based!");
					return 0;
				}

				if (AsNumberEnd < AsNumberStart)
				{
					OutErrorMessage = FString::Printf(TEXT("End of range must be greater than start of range (\"%s\")!"), *CommaPart);
					return 0;
				}

				for (int32 i = AsNumberStart; i < (AsNumberEnd + 1); ++i)
				{
					SetBit(i - 1);
				}
			}
		}
		return Bitfield;
	}

	bool FMidiStreamEventTrackChannelFilter::SetMidiChannelFilterFromString(const FString& FilterString, FString& OutErrorMessage)
	{
		MidiChannelFilter = (uint16)StringToBitfield(FilterString, OutErrorMessage);
		return OutErrorMessage.IsEmpty();
	}

	bool FMidiStreamEventTrackChannelFilter::SetTrackFilterFromString(const FString& FilterString, FString& OutErrorMessage)
	{
		TrackFilter = (uint64)StringToBitfield(FilterString, OutErrorMessage);
		return OutErrorMessage.IsEmpty();
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
