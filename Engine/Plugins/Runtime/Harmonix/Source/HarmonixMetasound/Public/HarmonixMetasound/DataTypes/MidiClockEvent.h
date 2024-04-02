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
		};

		struct FLoop
		{
			int32 LoopStartTick;
			int32 LoopEndTick;
		};

		struct FSeekTo
		{
			int32 FromTick;
			int32 ToTick;
		};

		struct FSeekThru
		{
			int32 FromTick;
			int32 ThruTick;
		};

		struct FAdvanceThru
		{
			int32 FromTick;
			int32 ThruTick;
			bool IsPreRoll;
		};

		struct FTempoChange
		{
			int32 Tick;
			float Tempo;
		};

		struct FTimeSignatureChange
		{
			int32 Tick;
			FTimeSignature TimeSignature;
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
