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
		const bool  IsPreRoll;
		const bool  ForceNoBroadcast;
		const int32 BlockFrameIndex;
		const int32 StartTick;
		const int32 EndTick;


		static FMidiClockEvent MakeResetEvent(int32 InBlockFrameIndex, int32 Tick, bool ForceNoBroadcast = false);
		static FMidiClockEvent MakeLoopEvent(int32 InBlockFrameIndex, int32 InStartTick, int32 InEndTick);
		static FMidiClockEvent MakeSeekToEvent(int32 InBlockFrameIndex, int32 Tick);
		static FMidiClockEvent MakeSeekThruEvent(int32 InBlockFrameIndex, int32 Tick);
		static FMidiClockEvent MakeAdvanceThruEvent(int32 InBlockFrameIndex, int32 Tick, bool IsPreRoll);

	private:

		FMidiClockEvent(EType InType, int32 InBlockFrameIndex, int32 InStartTick, int32 InEndTick, bool InIsPreRoll, bool InBroadcastEvents);
	};

};