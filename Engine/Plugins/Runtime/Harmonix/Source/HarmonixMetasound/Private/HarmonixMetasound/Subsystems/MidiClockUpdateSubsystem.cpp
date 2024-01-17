// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Subsystems/MidiClockUpdateSubsystem.h"

#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "Misc/ScopeLock.h"

bool UMidiClockUpdateSubsystem::IsTickable() const
{
	FScopeLock Lock{ &TrackedClocksMutex };
	
	return TrackedClocks.Num() > 0;
}

void UMidiClockUpdateSubsystem::Tick(float DeltaTime)
{
	FScopeLock Lock{ &TrackedClocksMutex };

	for (HarmonixMetasound::FMidiClock* Clock : TrackedClocks)
	{
		Clock->UpdateLowResCursors();
	}
}

TStatId UMidiClockUpdateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMidiClockUpdateSubsystem, STATGROUP_Tickables);
}

void UMidiClockUpdateSubsystem::TrackClock(HarmonixMetasound::FMidiClock* Clock)
{
	check(nullptr != Clock);
	
	FScopeLock Lock{ &TrackedClocksMutex };

	TrackedClocks.AddUnique(Clock);
}

void UMidiClockUpdateSubsystem::StopTrackingClock(HarmonixMetasound::FMidiClock* Clock)
{
	FScopeLock Lock{ &TrackedClocksMutex };

	TrackedClocks.Remove(Clock);
}

