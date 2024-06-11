// Copyright Epic Games, Inc. All Rights Reserved.

#include "DaySequencePlayer.h"
#include "DaySequence.h"
#include "DaySequenceActor.h"
#include "DaySequenceSpawnRegister.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DaySequencePlayer)

UDaySequencePlayer::UDaySequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UDaySequencePlayer::Initialize(UDaySequence* InDaySequence, ADaySequenceActor* Owner, const FMovieSceneSequencePlaybackSettings& Settings)
{
	WeakOwner = Owner;

	SpawnRegister = MakeShareable(new FDaySequenceSpawnRegister);
	UMovieSceneSequencePlayer::Initialize(InDaySequence, Settings);
}

bool UDaySequencePlayer::CanPlay() const
{
	return WeakOwner.IsValid();
}

UObject* UDaySequencePlayer::GetPlaybackContext() const
{
	return WeakOwner.Get();
}

void UDaySequencePlayer::RewindForReplay()
{
	// Stop the sequence when starting to seek through a replay. This restores our state to be unmodified
	// in case the replay is seeking to before playback. If we're in the middle of playback after rewinding,
	// the replay will feed the correct packets to synchronize our playback time and state.
	Stop();

	NetSyncProps.LastKnownPosition = FFrameTime(0);
	NetSyncProps.LastKnownStatus = EMovieScenePlayerStatus::Stopped;
	NetSyncProps.LastKnownNumLoops = 0;
	NetSyncProps.LastKnownSerialNumber = 0;
}

