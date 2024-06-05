// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HarmonixMetasound/DataTypes/MusicTransport.h"

namespace HarmonixMetasound
{
	namespace MidiClockMessageTypes
	{
		struct FLoop
		{
			int32 FirstTickInLoop;
			int32 LengthInTicks;

			FLoop(const int32 FirstTickInLoop, const int32 LengthInTicks)
				: FirstTickInLoop(FirstTickInLoop)
				, LengthInTicks(LengthInTicks)
			{}
		};

		struct FSeek
		{
			int32 LastTickProcessedBeforeSeek;
			int32 NewNextTick;

			FSeek(const int32 LastTickProcessedBeforeSeek, const int32 NewNextTick)
				: LastTickProcessedBeforeSeek(LastTickProcessedBeforeSeek)
				, NewNextTick(NewNextTick)
			{}
		};

		struct FAdvance
		{
			int32 FirstTickToProcess;
			int32 NumberOfTicksToProcess;

			FAdvance(const int32 FirstTickToProcess, const int32 NumberOfTicksToProcess)
				: FirstTickToProcess(FirstTickToProcess)
				, NumberOfTicksToProcess(NumberOfTicksToProcess)
			{}

			int32 LastTickToProcess() const { return FirstTickToProcess + NumberOfTicksToProcess - 1; }

			bool ContainsTick(int32 InTick) const { return InTick >= FirstTickToProcess && InTick < (FirstTickToProcess + NumberOfTicksToProcess); }
		};

		struct FTempoChange
		{
			int32 Tick;
			float Tempo;

			FTempoChange(const int32 Tick, const float Tempo)
				: Tick(Tick)
				, Tempo(Tempo)
			{}

			bool ContainsTick(int32 InTick) const { return InTick == Tick; }
		};

		struct FTimeSignatureChange
		{
			int32 Tick;
			FTimeSignature TimeSignature;

			FTimeSignatureChange(const int32 Tick, FTimeSignature&& TimeSignature)
				: Tick(Tick)
				, TimeSignature(MoveTemp(TimeSignature))
			{}

			bool ContainsTick(int32 InTick) const { return InTick == Tick; }
		};

		struct FTransportChange
		{
			EMusicPlayerTransportState TransportState;

			FTransportChange(EMusicPlayerTransportState NewTransportState)
				: TransportState(NewTransportState)
			{}
		};

		struct FSpeedChange
		{
			float Speed;

			FSpeedChange(float NewSpeed)
				: Speed(NewSpeed)
			{}
		};
	}

	using FMidiClockMsg = TVariant<
		MidiClockMessageTypes::FLoop,
		MidiClockMessageTypes::FSeek,
		MidiClockMessageTypes::FAdvance,
		MidiClockMessageTypes::FTempoChange,
		MidiClockMessageTypes::FTimeSignatureChange,
		MidiClockMessageTypes::FTransportChange,
		MidiClockMessageTypes::FSpeedChange
	>;
	
	struct HARMONIXMETASOUND_API FMidiClockEvent
	{
		const int32 BlockFrameIndex;
		FMidiClockMsg Msg;

		template<typename T>
		FMidiClockEvent(const int32 InBlockFrameIndex, T&& Msg)
			: BlockFrameIndex(InBlockFrameIndex)
			, Msg(TInPlaceType<T>(), MoveTemp(Msg))
		{
		}

		template<typename T>
		bool IsType() const	{ return Msg.IsType<T>(); }

		template<typename T>
		const T* TryGet() const { return Msg.TryGet<T>(); }

		template<typename T>
		T* TryGet() { return Msg.TryGet<T>(); }
	};
};
