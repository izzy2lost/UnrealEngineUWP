// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiClockEvent.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"


namespace HarmonixMetasound
{
	FMidiClockEvent FMidiClockEvent::MakeResetEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ToTick, bool ForceNoBroadcast)
	{
		return FMidiClockEvent(EType::Reset, InBlockFrameIndex, FromTick, ToTick, false, ForceNoBroadcast);
	}

	FMidiClockEvent FMidiClockEvent::MakeLoopEvent(int32 InBlockFrameIndex, int32 InStartTick, int32 InEndTick)
	{
		return FMidiClockEvent(EType::Loop, InBlockFrameIndex, InStartTick, InEndTick);
	}

	FMidiClockEvent FMidiClockEvent::MakeSeekToEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ToTick)
	{
		return FMidiClockEvent(EType::SeekTo, InBlockFrameIndex, FromTick, ToTick);
	}

	FMidiClockEvent FMidiClockEvent::MakeSeekThruEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ThruTick)
	{
		return FMidiClockEvent(EType::SeekThru, InBlockFrameIndex, FromTick, ThruTick);
	}

	FMidiClockEvent FMidiClockEvent::MakeAdvanceThruEvent(int32 InBlockFrameIndex, int32 FromTick, int32 ThruTick, bool IsPreRoll)
	{
		return FMidiClockEvent(EType::AdvanceThru, InBlockFrameIndex, FromTick, ThruTick, IsPreRoll);
	}

	FMidiClockEvent::FMidiClockEvent(EType InType, int32 InBlockFrameIndex, int32 InTick1, int32 InTick2, bool InIsPreRoll, bool InForceNoBroadcast)
		: Type(InType)
		, IsPreRoll(InIsPreRoll)
		, ForceNoBroadcast(InForceNoBroadcast)
		, BlockFrameIndex(InBlockFrameIndex)
		, Tick1(InTick1)
		, Tick2(InTick2)
	{}

}
