// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/MidiVoiceId.h"
#include "Logging/LogMacros.h"
#include "MetasoundDataReference.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundVariable.h"

#include "MidiClock.h"

#include <functional>
#include <atomic>

namespace HarmonixMetasound
{
	DECLARE_LOG_CATEGORY_EXTERN(LogMidiStreamDataType, Log, All)

	struct HARMONIXMETASOUND_API FMidiStreamEvent
	{
		int32        BlockSampleFrameIndex  = 0;
		float        BlockSampleFrameOffset = 0.0f;
		int32        TrackIndex             = 0;
		int32        AuthoredMidiTick       = 0;
		int32        CurrentMidiTick        = 0;
		float        MsOffset               = 0.0f;
		FMidiMsg     MidiMessage;

		FMidiStreamEvent(const FMidiVoiceGeneratorBase* Owner, const FMidiMsg& Message)
			: MidiMessage(Message)
		{
			VoiceId = FMidiVoiceId(Owner, MidiMessage);
		}

		FMidiStreamEvent(uint32 OwnerId, const FMidiMsg& Message)
			: MidiMessage(Message)
		{
			VoiceId = FMidiVoiceId(OwnerId, MidiMessage);
		}

		FMidiVoiceId GetVoiceId() const { return VoiceId; }
		void SetVoiceId(FMidiVoiceId InVoiceId) { VoiceId = InVoiceId; }

	private:
		FMidiVoiceId VoiceId;
	};

	struct HARMONIXMETASOUND_API FMidiStreamEventTrackChannelFilter
	{
		uint16 MidiChannelFilter;
		uint64 TrackFilter;

		/** 
		* Parses the FilterString to a Bitfield and assigns it to the MidiChannelFilter
		* if it fails, will assign MidiChannelFilter to 0
		*/
		bool SetMidiChannelFilterFromString(const FString& FilterString, FString& OutErrorMessage);

		/**
		* Parses the FilterString to a Bitfield and assigns it to the TrackFilter
		* if it fails, will assign TrackFilter to 0
		*/
		bool SetTrackFilterFromString(const FString& FilterString, FString& OutErrorMessage);

		bool operator()(const FMidiStreamEvent& Event) const
		{
			if (!(TrackFilter & (uint64(1) << Event.TrackIndex)))
			{
				return false;
			}

			return !Event.MidiMessage.IsStd() || (MidiChannelFilter & (uint16(1) << Event.MidiMessage.GetStdChannel()));
		}

	private:
		uint64 StringToBitfield(const FString& InString, FString& OutErrorMesage);
	};


	class HARMONIXMETASOUND_API FMidiStream
	{
	public:

		FMidiStream(const Metasound::FOperatorSettings& InSettings);

		void SetClockSource(const FMidiClockReadRef& ClockSource)
		{
			MidiClockSource.Emplace(ClockSource);
		}

		void ResetClockSource()
		{
			MidiClockSource.Reset();
		}

		using MidiSinkFunction = std::function<void()>;
		void ExecuteBlock(MidiSinkFunction Destination) const
		{
		}

		void PrepareBlock();

		void AddTransportStateChangeMessage(int32 SampleFrameIndexInBlock, EMusicPlayerTransportState State);
		void AddMidiEvent(const FMidiStreamEvent& Event);
		void InsertMidiEvent(const FMidiStreamEvent& Event);
		void AddNoteOffEventOrCancelPendingNoteOn(const FMidiStreamEvent& Event);
		void InsertNoteOffEventOrCancelPendingNoteOn(const FMidiStreamEvent& Event);

		void SetMidiFile(const FMidiFileProxyPtr& MidiFile) { MidiFileSourceOfEvents = MidiFile; }
		const FString* GetMidiTrackText(int32 TrackNumber, int32 TextIndex) const;

		const TArray<FMidiTimestampTransportState>& GetTransportChangesInBlock() const
		{
			return TransportChangesInBlock;
		}
		const TArray<FMidiStreamEvent>& GetEventsInBlock() const
		{
			return EventsInBlock;
		}

		const FMidiClockReadRef* GetMidiClockSource() const 
		{
			return MidiClockSource.GetPtrOrNull();
		}

		// Forward declare the read ref for copying other streams
		using FMidiStreamReadRef = Metasound::TDataReadReference<FMidiStream>;

		// Copies all midi and transport events from the in stream to this stream. Obliterates any
		// transport/midi events already in this stream!
		void Copy(FMidiStreamReadRef InStream, bool EventsOnly = false);

		// Copies all midi and transport events from the in streams to this stream. Obliterates any
		// transport/midi events already in this stream!
		void Copy(const TArray<FMidiStreamReadRef>& InStreams, bool EventsOnly = false);

		// Copies all transport events, and midi events that match the predicate, from the in stream to this stream.
		// Obliterates any transport/midi events already in this stream!
		template<typename T>
		void Copy(FMidiStreamReadRef InStream, const T& Predicate, bool EventsOnly = false)
		{
			if (GetMidiClockSource() && InStream->GetMidiClockSource() && GetMidiClockSource()->Get() != InStream->GetMidiClockSource()->Get())
			{
				UE_LOG(LogMidiStreamDataType, Warning, TEXT("Cannot copy midi events from one stream to another if they have different clocks."));
				return;
			}
			if (!EventsOnly)
			{
				CopyTransportEvents(InStream);
			}
			CopyMidiEvents(InStream, Predicate);
		}

		// Copies all transport events, and midi events that match the predicate, from the in streams to this stream.
		// Obliterates any transport/midi events already in this stream!
		template<typename T>
		void Copy(const TArray<FMidiStreamReadRef>& InStreams, const T& Predicate, bool EventsOnly = false)
		{
			if (!InStreams.Num())
			{
				return;
			}

			if (InStreams.Num() == 1)
			{
				Copy(InStreams[0], Predicate);
				return;
			}

			TArray<FMidiStreamReadRef> SameClockStreams;
			FilterArrayToStreamsWithTheSameClock(InStreams, SameClockStreams);
			if (!EventsOnly)
			{
				CopyTransportEvents(SameClockStreams);
			}
			CopyMidiEvents(SameClockStreams, Predicate);
		}

		// Copies all midi and transport events from the in stream to this stream that match the predicate. Obliterates any
		// transport/midi events already in this stream!
		template<typename FILTER, typename TRANSFORMER>
		void Copy(FMidiStreamReadRef InStream, const FILTER& Predicate, const TRANSFORMER& Transformer, bool EventsOnly = false)
		{
			if (GetMidiClockSource() && InStream->GetMidiClockSource() && GetMidiClockSource()->Get() != InStream->GetMidiClockSource()->Get())
			{
				UE_LOG(LogMidiStreamDataType, Warning, TEXT("Cannot copy midi events from one stream to another if they have different clocks."));
				return;
			}
			if (!EventsOnly)
			{
				CopyTransportEvents(InStream);
			}
			CopyMidiEvents(InStream, Predicate, Transformer);
		}

		int32 GetTicksPerQuarterNote() const { return TicksPerQuarterNote; }
		void SetTicksPerQuarterNote(int32 InTicksPerQuarterNote) { TicksPerQuarterNote = InTicksPerQuarterNote; }

		// Returns an array of only the in streams that have the same clock as this stream
		void FilterArrayToStreamsWithTheSameClock(const TArray<FMidiStreamReadRef>& InStreams, TArray<FMidiStreamReadRef>& OutStreams);
		// Copies all transport events from the in streams to this. Obliterates any existing transport events.
		void CopyTransportEvents(const TArray<FMidiStreamReadRef>& InStreams);
		// Copies all transport events from the in stream to this. Obliterates any existing transport events.
		void CopyTransportEvents(const FMidiStreamReadRef& InStream);
		// Copies all midi events from the in streams to this. Obliterates and existing midi events.
		void CopyMidiEvents(const TArray<FMidiStreamReadRef>& InStreams);
		// Merges all midi events from the in streams to this.
		void MergeMidiEvents(const TArray<FMidiStreamReadRef>& InStreams);
		// Copies all midi events from the in stream to this. Obliterates and existing midi events.
		void CopyMidiEvents(const FMidiStreamReadRef& InStream);
		// Merges all midi events from the in stream to this.
		void MergeMidiEvents(const FMidiStreamReadRef& InStream);

		// Copies all midi events that match the predicate from the in stream to this. Obliterates and existing midi events.
		template<typename T>
		void CopyMidiEvents(const FMidiStreamReadRef& InStream, const T& Predicate)
		{
			const TArray<FMidiStreamEvent>& MidiEvents = InStream->GetEventsInBlock();
			if (!ensureAlwaysMsgf(EventsInBlock.Num()==0, TEXT("Existing midi events in midi stream are being destroyed during copy!")))
			{
				EventsInBlock.Empty(FMath::Max(MidiEvents.Num(), 32));
				ActiveVoices.Reset();
			}
			for (const FMidiStreamEvent& Event : MidiEvents)
			{
				if (Predicate(Event))
				{
					AddMidiEvent(Event);
				}
			}
		}

		// Merges all midi events that match the predicate from the in stream to this. 
		template<typename T>
		void MergeMidiEvents(const FMidiStreamReadRef& InStream, const T& Predicate)
		{
			const TArray<FMidiStreamEvent>& MidiEvents = InStream->GetEventsInBlock();
			for (const FMidiStreamEvent& Event : MidiEvents)
			{
				if (Predicate(Event))
				{
					InsertMidiEvent(Event);
				}
			}
		}

		// Copies all midi events that match the predicate from the in stream to this. Obliterates and existing midi events.
		template<typename T>
		void CopyMidiEvents(const TArray<FMidiStreamReadRef>& InStreams, const T& Predicate)
		{
			if (!ensureAlwaysMsgf(EventsInBlock.Num() == 0, TEXT("Existing midi events in midi stream are being destroyed during copy!")))
			{
				EventsInBlock.Empty(32);
				ActiveVoices.Reset();
			}

			// Define the type so that we can store iterators in an array
			using MidiStreamIterator = TArray<FMidiStreamEvent>::RangedForConstIteratorType;

			TArray<MidiStreamIterator> WalkingIterators;
			TArray<MidiStreamIterator> IteratorEnds;

			for (int32 i = 0; i < InStreams.Num(); ++i)
			{
				WalkingIterators.Add(InStreams[i]->GetEventsInBlock().begin());
				IteratorEnds.Add(InStreams[i]->GetEventsInBlock().end());
			}

			while (1)
			{
				int32 StreamWithEarliestEvent = -1;
				int32 EarliestMidiTick = MAX_int32;

				for (int32 IteratorIndex = 0; IteratorIndex < WalkingIterators.Num(); ++IteratorIndex)
				{
					if (WalkingIterators[IteratorIndex] != IteratorEnds[IteratorIndex])
					{
						if ((*WalkingIterators[IteratorIndex]).AuthoredMidiTick < EarliestMidiTick)
						{
							EarliestMidiTick = (*WalkingIterators[IteratorIndex]).AuthoredMidiTick;
							StreamWithEarliestEvent = IteratorIndex;
						}
					}
				}

				// done?
				if (StreamWithEarliestEvent == -1)
				{
					break;
				}

				if (Predicate(*WalkingIterators[StreamWithEarliestEvent]))
				{
					AddMidiEvent(*WalkingIterators[StreamWithEarliestEvent]);
				}
				++WalkingIterators[StreamWithEarliestEvent];
			}
		}

		// Merges all midi events that match the predicate from the in stream to this.
		template<typename T>
		void MergeMidiEvents(const TArray<FMidiStreamReadRef>& InStreams, const T& Predicate)
		{
			// Define the type so that we can store iterators in an array
			using MidiStreamIterator = TArray<FMidiStreamEvent>::RangedForConstIteratorType;

			TArray<MidiStreamIterator> WalkingIterators;
			TArray<MidiStreamIterator> IteratorEnds;

			for (int32 i = 0; i < InStreams.Num(); ++i)
			{
				WalkingIterators.Add(InStreams[i]->GetEventsInBlock().begin());
				IteratorEnds.Add(InStreams[i]->GetEventsInBlock().end());
			}

			while (1)
			{
				int32 StreamWithEarliestEvent = -1;
				int32 EarliestBlockSampleFrameIndex = MAX_int32;

				for (int32 IteratorIndex = 0; IteratorIndex < WalkingIterators.Num(); ++IteratorIndex)
				{
					if (WalkingIterators[IteratorIndex] != IteratorEnds[IteratorIndex])
					{
						if ((*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex == EarliestBlockSampleFrameIndex)
						{
							if ((*WalkingIterators[StreamWithEarliestEvent]).MidiMessage.IsNoteOn() && (*WalkingIterators[IteratorIndex]).MidiMessage.IsNoteOff())
							{
								StreamWithEarliestEvent = IteratorIndex;
							}
						}
						else if ((*WalkingIterators[IteratorIndex]).BlockSampleFrameIndex < EarliestBlockSampleFrameIndex)
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

				if (Predicate(*WalkingIterators[StreamWithEarliestEvent]))
				{
					InsertMidiEvent(*WalkingIterators[StreamWithEarliestEvent]);
				}
				++WalkingIterators[StreamWithEarliestEvent];
			}
		}

		// Copies all midi events that match the predicate from the in stream to this. Optionally transforms the events. Obliterates and existing midi events.
		template<typename FILTER, typename TRANSFORMER>
		void CopyMidiEvents(const FMidiStreamReadRef& InStream, const FILTER& Predicate, const TRANSFORMER& Transformer)
		{
			const TArray<FMidiStreamEvent>& MidiEvents = InStream->GetEventsInBlock();
			EventsInBlock.Empty(FMath::Max(MidiEvents.Num(), 32));
			ActiveVoices.Reset();
			
			for (const FMidiStreamEvent& Event : MidiEvents)
			{
				if (Predicate(Event))
				{
					AddMidiEvent(Transformer(Event));
				}
			}
		}

		// Copies all midi events that match the predicate from the in stream to this. Optionally transforms the events. Obliterates and existing midi events.
		template<typename FILTER, typename TRANSFORMER>
		void MergeMidiEvents(const FMidiStreamReadRef& InStream, const FILTER& Predicate, const TRANSFORMER& Transformer)
		{
			const TArray<FMidiStreamEvent>& MidiEvents = InStream->GetEventsInBlock();
			for (const FMidiStreamEvent& Event : MidiEvents)
			{
				if (Predicate(Event))
				{
					InsertMidiEvent(Transformer(Event));
				}
			}
		}

	private:
		FMidiFileProxyPtr MidiFileSourceOfEvents;
		int32 NumFramesPerBlock = 0;
		Metasound::FSampleRate SampleRate  = 0;
		int32 TicksPerQuarterNote = MidiConstants::kTicksPerQuarterNoteInt;

		FMidiTimestampTransportState CurrentTransportState;
		TArray<FMidiTimestampTransportState> TransportChangesInBlock;

		TArray<FMidiStreamEvent> EventsInBlock;

		friend class FMidiVoiceTracker;
		void UpdateActiveVoice(const FMidiStreamEvent& Event);
		TSet<FMidiVoiceId> ActiveVoices;

		TOptional<FMidiClockReadRef> MidiClockSource;
	};

	// Declare aliases IN the namespace...
	DECLARE_METASOUND_DATA_REFERENCE_ALIAS_TYPES(FMidiStream, FMidiStreamTypeInfo, FMidiStreamReadRef, FMidiStreamWriteRef)

	class HARMONIXMETASOUND_API FMidiVoiceTracker
	{
	public:
		using FKillVoiceFn = TFunctionRef<void(const FMidiVoiceId&)>;

		void Process(const FMidiStream& MidiStream, const FKillVoiceFn& KillVoiceFn);

	private:
		TSet<FMidiVoiceId> ActiveVoices;
	};
}

// Declare reference types OUT of the namespace...
DECLARE_METASOUND_DATA_REFERENCE_TYPES_NO_ALIASES(HarmonixMetasound::FMidiStream, HARMONIXMETASOUND_API)
