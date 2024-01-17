// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "Subsystems/EngineSubsystem.h"

#include "MidiClockUpdateSubsystem.generated.h"

namespace HarmonixMetasound
{
	class FMidiClock;
}

/**
 * Handles updating low-resolution play cursors associated with a MIDI clock.
 * Long story short, because FMidiClock instances do not have their lifecycle managed by the garbage collector,
 * we need a way to tick them on the game thread while avoiding races. This gives us a way to register clocks to be
 * ticked from within their constructors/destructor, and provides no user-facing API.
 */
UCLASS()
class HARMONIXMETASOUND_API UMidiClockUpdateSubsystem final : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Begin FTickableGameObject
	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

private:
	friend class HarmonixMetasound::FMidiClock;
	
	void TrackClock(HarmonixMetasound::FMidiClock* Clock);
	void StopTrackingClock(HarmonixMetasound::FMidiClock* Clock);

	mutable FCriticalSection TrackedClocksMutex;
	TArray<HarmonixMetasound::FMidiClock*> TrackedClocks;
};
