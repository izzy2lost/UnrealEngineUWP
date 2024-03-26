// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMidi/MidiPlayCursor.h"

namespace HarmonixMetasound
{
	struct HARMONIXMETASOUND_API FMidiClockEvent
	{
		enum class EType : uint8
		{
			Reset,
			Loop,
			SeekTo,
			SeekThru,
			AdvanceThru
		};

		const EType Type;

		// for "advance thru" events
		const bool  IsPreRoll;

		// for "reset" events
		const bool  ForceNoBroadcast;
		const int32 BlockFrameIndex;

		// FromTick when event is SeekTo, SeekThru, AdvanceThru or Reset;
		// LoopStartTick when OnLoop
		const int32 Tick1;

		// ToTick when SeekTo, Reset;
		// ThruTick when SeekThru, AdvanceThru;
		// LoopEndTick when OnLoop
		const int32 Tick2;

		static FMidiClockEvent MakeResetEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ToTick, bool ForceNoBroadcast = false);
		static FMidiClockEvent MakeLoopEvent(int32 InBlockFrameIndex, int32 LoopStartTick, int32 LoopEndTick);
		static FMidiClockEvent MakeSeekToEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ToTick);
		static FMidiClockEvent MakeSeekThruEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ThruTick);
		static FMidiClockEvent MakeAdvanceThruEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ThruTick, bool IsPreRoll);

	private:

		FMidiClockEvent(EType InType, int32 InBlockFrameIndex, int32 InTick1, int32 InTick2, bool InIsPreRoll = false, bool InForceNoBroadcast = false);
	};

};
