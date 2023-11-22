// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Evaluation/MovieScenePlaybackCapabilities.h"
#include "MovieSceneSequenceID.h"

namespace UE::MovieScene
{

struct FSharedPlaybackState;

/**
 * Playback capability for sequences that have a director blueprint.
 */
struct MOVIESCENE_API ISequenceDirectorPlaybackCapability
{
	/** Playback capability ID */
	static TPlaybackCapabilityID<ISequenceDirectorPlaybackCapability> ID;

	virtual ~ISequenceDirectorPlaybackCapability() {}

	/** Remove all director blueprint instances */
	virtual void ResetDirectorInstances() = 0;

	/** Gets a new or existing director blueprint instance for the given root or sub sequence */
	virtual UObject* GetOrCreateDirectorInstance(TSharedRef<const FSharedPlaybackState> SharedPlaybackState, FMovieSceneSequenceIDRef SequenceID) = 0;
};

}  // namespace UE::MovieScene

