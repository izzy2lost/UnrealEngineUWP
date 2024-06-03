// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MovieSceneReplaceableDirectorBlueprintBinding.h"
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
#include "MovieSceneDynamicBindingInvoker.h"

#define LOCTEXT_NAMESPACE "MovieScene"

#if WITH_EDITOR

FText UMovieSceneReplaceableDirectorBlueprintBinding::GetBindingTypePrettyName() const
{
	return LOCTEXT("MovieSceneReplaceableDirectorBlueprintBinding", "Replaceable from Director Blueprint");
}


void UMovieSceneReplaceableDirectorBlueprintBinding::OnBindingAddedOrChanged(UMovieScene& OwnerMovieScene)
{
	if ((PreviewSpawnableType == nullptr && PreviewSpawnable)
		|| (PreviewSpawnableType != nullptr && (!PreviewSpawnable || PreviewSpawnable->GetClass() != PreviewSpawnableType)))
	{
		if (PreviewSpawnableType == nullptr)
		{
			PreviewSpawnable = nullptr;
		}
		else
		{
			PreviewSpawnable = NewObject<UMovieSceneSpawnableBindingBase>(&OwnerMovieScene, PreviewSpawnableType, NAME_None, RF_Transactional);
		}
	}
}
#endif

FMovieSceneBindingResolveResult UMovieSceneReplaceableDirectorBlueprintBinding::ResolveRuntimeBindingInternal(const FMovieSceneBindingResolveParams& ResolveParams, int32 BindingIndex, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) const
{
	FMovieSceneBindingResolveResult ResolveResult;
	FMovieSceneDynamicBindingResolveResult DynamicResolveResult = FMovieSceneDynamicBindingInvoker::ResolveDynamicBinding(SharedPlaybackState, ResolveParams.Sequence, ResolveParams.SequenceID, ResolveParams.ObjectBindingID, DynamicBinding);
	ResolveResult.Object = DynamicResolveResult.Object;
	return ResolveResult;
}

#undef LOCTEXT_NAMESPACE