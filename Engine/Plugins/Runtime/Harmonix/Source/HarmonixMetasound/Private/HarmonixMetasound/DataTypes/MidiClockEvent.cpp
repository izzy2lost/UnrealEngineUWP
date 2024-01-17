// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiClockEvent.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"


namespace HarmonixMetasound
{
	FMidiClockEvent FMidiClockEvent::MakeResetEvent(int32 InBlockFrameIndex, int32 Tick, bool ForceNoBroadcast)
	{
		return FMidiClockEvent(EType::Reset, InBlockFrameIndex, Tick, Tick, false, ForceNoBroadcast);
	}

	FMidiClockEvent FMidiClockEvent::MakeLoopEvent(int32 InBlockFrameIndex, int32 InStartTick, int32 InEndTick)
	{
		return FMidiClockEvent(EType::Loop, InBlockFrameIndex, InStartTick, InEndTick, false, true);
	}

	FMidiClockEvent FMidiClockEvent::MakeSeekToEvent(int32 InBlockFrameIndex, int32 Tick)
	{
		return FMidiClockEvent(EType::SeekTo, InBlockFrameIndex, Tick, Tick, false, true);
	}

	FMidiClockEvent FMidiClockEvent::MakeSeekThruEvent(int32 InBlockFrameIndex, int32 Tick)
	{
		return FMidiClockEvent(EType::SeekThru, InBlockFrameIndex, Tick, Tick, false, true);
	}

	FMidiClockEvent FMidiClockEvent::MakeAdvanceThruEvent(int32 InBlockFrameIndex, int32 Tick, bool IsPreRoll)
	{
		return FMidiClockEvent(EType::AdvanceThru, InBlockFrameIndex, Tick, Tick, IsPreRoll, true);
	}

	FMidiClockEvent::FMidiClockEvent(EType InType, int32 InBlockFrameIndex, int32 InStartTick, int32 InEndTick, bool InIsPreRoll, bool InForceNoBroadcast)
		: Type(InType)
		, IsPreRoll(InIsPreRoll)
		, ForceNoBroadcast(InForceNoBroadcast)
		, BlockFrameIndex(InBlockFrameIndex)
		, StartTick(InStartTick)
		, EndTick(InEndTick)
	{}

}