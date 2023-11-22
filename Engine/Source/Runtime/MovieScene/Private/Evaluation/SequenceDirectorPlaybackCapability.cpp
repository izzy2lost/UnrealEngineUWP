// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/SequenceDirectorPlaybackCapability.h"

#include "EntitySystem/MovieSceneSharedPlaybackState.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"

namespace UE::MovieScene
{

TPlaybackCapabilityID<ISequenceDirectorPlaybackCapability> ISequenceDirectorPlaybackCapability::ID = TPlaybackCapabilityID<ISequenceDirectorPlaybackCapability>::Register();

}  // namespace UE::MovieScene

