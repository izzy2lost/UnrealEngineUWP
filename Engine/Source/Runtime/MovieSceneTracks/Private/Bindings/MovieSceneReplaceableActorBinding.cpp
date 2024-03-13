// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MovieSceneReplaceableActorBinding.h"
#include "EntitySystem/MovieSceneSharedPlaybackState.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "MovieScenePossessable.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneBindingReferences.h"
#include "MovieSceneCommonHelpers.h"
#include "Bindings/MovieSceneSpawnableActorBinding.h"

#define LOCTEXT_NAMESPACE "MovieScene"

#if WITH_EDITOR

FText UMovieSceneReplaceableActorBinding::GetBindingTypePrettyName() const
{
	return LOCTEXT("MovieSceneReplaceableActorBinding", "Replaceable Actor");
}
#endif

TSubclassOf<UMovieSceneSpawnableBindingBase> UMovieSceneReplaceableActorBinding::GetInnerSpawnableClass() const
{
	return UMovieSceneSpawnableActorBinding::StaticClass();
}

#undef LOCTEXT_NAMESPACE