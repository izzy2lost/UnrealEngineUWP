// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MovieSceneSpawnableBinding.h"
#include "MovieSceneSpawnRegister.h"
#include "EntitySystem/MovieSceneSharedPlaybackState.h"
#include "MovieScene.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "MovieSceneBindingReferences.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"
#include "Sections/MovieSceneBindingLifetimeSection.h"
#include "Styling/SlateBrush.h"
#include "Styling/AppStyle.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "FPossessableModel"


UObject* UMovieSceneSpawnableBindingBase::SpawnObject(const FGuid& BindingId, int32 BindingIndex, UMovieScene& MovieScene, FMovieSceneSequenceIDRef TemplateID, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState)
{
	UWorld* WorldContext = GetWorldContext(SharedPlaybackState);

	if (WorldContext == nullptr)
	{
		UE_LOG(LogMovieScene, Warning, TEXT("Can't find world to spawn '%s' into, defaulting to Persistent level"), *MovieScene.GetName());

		WorldContext = GWorld;
	}

	FName SpawnName = GetSpawnName(BindingId, MovieScene, TemplateID, SharedPlaybackState);

	// If there's an object that already exists with the requested name, it needs to be renamed (it's probably pending kill)
	if (!SpawnName.IsNone())
	{
		UObject* ExistingObject = StaticFindObjectFast(nullptr, WorldContext->PersistentLevel.Get(), SpawnName);
		if (ExistingObject)
		{
			FName DefunctName = MakeUniqueObjectName(WorldContext->PersistentLevel.Get(), ExistingObject->GetClass());
			ExistingObject->Rename(*DefunctName.ToString(), nullptr, REN_ForceNoResetLoaders);
		}
	}

	// Spawn Object Internal

	UObject* SpawnedObject = SpawnObjectInternal(WorldContext, SpawnName, BindingId, BindingIndex, MovieScene, TemplateID, SharedPlaybackState);

	if (!SpawnedObject)
	{
		return nullptr;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly set RF_Transactional on spawned objects so we can undo/redo properties on them.
		SpawnedObject->SetFlags(RF_Transactional);
	}
#endif

	// Allows derived classes to perform post-spawn logic such as mesh setup on actors.
	PostSpawnObject(SpawnedObject, WorldContext, BindingId, BindingIndex, MovieScene, TemplateID, SharedPlaybackState);

	return SpawnedObject;
}

void UMovieSceneSpawnableBindingBase::DestroySpawnedObject(UObject* Object)
{
	if (!Object)
	{
		return;
	}
#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly remove RF_Transactional on spawned objects since we don't want to transact spawn/destroy events
		Object->ClearFlags(RF_Transactional);
	}
#endif

	DestroySpawnedObjectInternal(Object);
}

#if WITH_EDITOR
void UMovieSceneSpawnableBindingBase::SetupDefaults(UObject* SpawnedObject, FGuid ObjectBindingId, UMovieScene& OwnerMovieScene, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState)
{
	if (!SpawnedObject)
	{
		return;
	}
	Super::SetupDefaults(SpawnedObject, ObjectBindingId, OwnerMovieScene, SharedPlaybackState);
	// Ensure it has a binding lifetime track
	UMovieSceneBindingLifetimeTrack* BindingLifetimeTrack = Cast<UMovieSceneBindingLifetimeTrack>(OwnerMovieScene.FindTrack(UMovieSceneBindingLifetimeTrack::StaticClass(), ObjectBindingId, NAME_None));
	if (!BindingLifetimeTrack)
	{
		BindingLifetimeTrack = Cast<UMovieSceneBindingLifetimeTrack>(OwnerMovieScene.AddTrack(UMovieSceneBindingLifetimeTrack::StaticClass(), ObjectBindingId));
	}

	if (BindingLifetimeTrack && BindingLifetimeTrack->GetAllSections().IsEmpty())
	{
		UMovieSceneBindingLifetimeSection* BindingLifetimeSection = Cast<UMovieSceneBindingLifetimeSection>(BindingLifetimeTrack->CreateNewSection());
		BindingLifetimeSection->SetRange(TRange<FFrameNumber>::All());
		BindingLifetimeTrack->AddSection(*BindingLifetimeSection);
	}
}

const FSlateBrush* UMovieSceneSpawnableBindingBase::GetBindingTrackCustomIconOverlay() const
{
	return FAppStyle::GetBrush("Sequencer.SpawnableIconOverlay");
}

FText UMovieSceneSpawnableBindingBase::GetBindingTrackIconTooltip() const
{
	return LOCTEXT("CustomSpawnableTooltip", "This item is spawned by sequencer by a custom spawnable binding according to this object's binding lifetime track.");
}

#endif

UWorld* UMovieSceneSpawnableBindingBase::GetWorldContext(TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) const
{
	UObject* PlaybackContext = SharedPlaybackState->GetPlaybackContext();
	return PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
}

FMovieSceneBindingResolveResult UMovieSceneSpawnableBindingBase::ResolveBinding(const FMovieSceneBindingResolveParams& ResolveParams, int32 BindingIndex, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) const
{
	FMovieSceneBindingResolveResult Result;
	const FMovieSceneSpawnRegister* SpawnRegister = SharedPlaybackState->FindCapability<FMovieSceneSpawnRegister>();
	UObject* SpawnedObject = SpawnRegister ? SpawnRegister->FindSpawnedObject(ResolveParams.ObjectBindingID, ResolveParams.SequenceID, BindingIndex).Get() : nullptr;
	if (SpawnedObject)
	{
		Result.Object = SpawnedObject;
	}
	return Result;
}

const UMovieSceneSpawnableBindingBase* UMovieSceneSpawnableBindingBase::AsSpawnable(TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) const
{
	return Cast<UMovieSceneSpawnableBindingBase>(this);
}

#undef LOCTEXT_NAMESPACE