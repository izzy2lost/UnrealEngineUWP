// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMidi/MidiPlayCursor.h"

namespace HarmonixMetasound
{
	namespace MidiClockMessageTypes
	{
		struct FReset
		{
			int32 FromTick;
			int32 ToTick;
			bool ForceNoBroadcast;

			FReset(const int32 FromTick, const int32 ToTick, const bool ForceNoBroadcast)
				: FromTick(FromTick)
				, ToTick(ToTick)
				, ForceNoBroadcast(ForceNoBroadcast)
			{}
		};

		struct FLoop
		{
			int32 LoopStartTick;
			int32 LoopEndTick;

			FLoop(const int32 LoopStartTick, const int32 LoopEndTick)
				: LoopStartTick(LoopStartTick)
				, LoopEndTick(LoopEndTick)
			{}
		};

		struct FSeekTo
		{
			int32 FromTick;
			int32 ToTick;

			FSeekTo(const int32 FromTick, const int32 ToTick)
				: FromTick(FromTick)
				, ToTick(ToTick)
			{}
		};

		struct FSeekThru
		{
			int32 FromTick;
			int32 ThruTick;

			FSeekThru(const int32 FromTick, const int32 ThruTick)
				: FromTick(FromTick)
				, ThruTick(ThruTick)
			{}
		};

		struct FAdvanceThru
		{
			int32 FromTick;
			int32 ThruTick;
			bool IsPreRoll;

			FAdvanceThru(const int32 FromTick, const int32 ThruTick, const bool IsPreRoll)
				: FromTick(FromTick)
				, ThruTick(ThruTick)
				, IsPreRoll(IsPreRoll)
			{}
		};

		struct FTempoChange
		{
			int32 Tick;
			float Tempo;

			FTempoChange(const int32 Tick, const float Tempo)
				: Tick(Tick)
				, Tempo(Tempo)
			{}
		};

		struct FTimeSignatureChange
		{
			int32 Tick;
			FTimeSignature TimeSignature;

			FTimeSignatureChange(const int32 Tick, FTimeSignature&& TimeSignature)
				: Tick(Tick)
				, TimeSignature(MoveTemp(TimeSignature))
			{}
		};
	}

	using FMidiClockMsg = TVariant<
		MidiClockMessageTypes::FReset,
		MidiClockMessageTypes::FLoop,
		MidiClockMessageTypes::FSeekTo,
		MidiClockMessageTypes::FSeekThru,
		MidiClockMessageTypes::FAdvanceThru,
		MidiClockMessageTypes::FTempoChange,
		MidiClockMessageTypes::FTimeSignatureChange
	>;
	
	struct HARMONIXMETASOUND_API FMidiClockEvent
	{
		const int32 BlockFrameIndex;
		const FMidiClockMsg Msg;

		template<typename T>
		FMidiClockEvent(const int32 InBlockFrameIndex, T&& Msg)
			: BlockFrameIndex(InBlockFrameIndex)
			, Msg(TInPlaceType<T>(), MoveTemp(Msg))
		{
		}
	};

};
