// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "IMovieScenePlaybackClient.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "Misc/ScopeRWLock.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceID.h"

namespace UE
{
namespace MovieScene
{

static FRWLock                          GGlobalPlayerRegistryLock;
static TSparseArray<IMovieScenePlayer*> GGlobalPlayerRegistry;
static TBitArray<> GGlobalPlayerUpdateFlags;

TPlaybackCapabilityID<FPlayerIndexPlaybackCapability> FPlayerIndexPlaybackCapability::ID = TPlaybackCapabilityID<FPlayerIndexPlaybackCapability>::Register();

IMovieScenePlayer* FPlayerIndexPlaybackCapability::GetPlayer(TSharedRef<const FSharedPlaybackState> Owner)
{
	if (FPlayerIndexPlaybackCapability* Cap = Owner->FindCapability<FPlayerIndexPlaybackCapability>())
	{
		return IMovieScenePlayer::Get(Cap->PlayerIndex);
	}
	return nullptr;
}

uint16 FPlayerIndexPlaybackCapability::GetPlayerIndex(TSharedRef<const FSharedPlaybackState> Owner)
{
	if (FPlayerIndexPlaybackCapability* Cap = Owner->FindCapability<FPlayerIndexPlaybackCapability>())
	{
		return Cap->PlayerIndex;
	}
	return (uint16)-1;
}

} // namespace MovieScene
} // namespace UE

UE::MovieScene::TPlaybackCapabilityID<IMovieScenePlaybackClient> IMovieScenePlaybackClient::ID = UE::MovieScene::TPlaybackCapabilityID<IMovieScenePlaybackClient>::Register();

IMovieScenePlayer::IMovieScenePlayer()
{
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);

	UE::MovieScene::GGlobalPlayerRegistry.Shrink();
	UniqueIndex = UE::MovieScene::GGlobalPlayerRegistry.Add(this);

	UE::MovieScene::GGlobalPlayerUpdateFlags.PadToNum(UniqueIndex + 1, false);
	UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex] = 0;
}

IMovieScenePlayer::~IMovieScenePlayer()
{	
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);

	UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex] = 0;
	UE::MovieScene::GGlobalPlayerRegistry.RemoveAt(UniqueIndex, 1);
}

IMovieScenePlayer* IMovieScenePlayer::Get(uint16 InUniqueIndex)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	check(UE::MovieScene::GGlobalPlayerRegistry.IsValidIndex(InUniqueIndex));
	return UE::MovieScene::GGlobalPlayerRegistry[InUniqueIndex];
}

void IMovieScenePlayer::Get(TArray<IMovieScenePlayer*>& OutPlayers, bool bOnlyUnstoppedPlayers)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	for (auto It = UE::MovieScene::GGlobalPlayerRegistry.CreateIterator(); It; ++It)
	{
		if (IMovieScenePlayer* Player = *It)
		{
			if (!bOnlyUnstoppedPlayers || Player->GetPlaybackStatus() != EMovieScenePlayerStatus::Stopped)
			{
				OutPlayers.Add(*It);
			}
		}
	}
}

void IMovieScenePlayer::SetIsEvaluatingFlag(uint16 InUniqueIndex, bool bIsUpdating)
{
	check(UE::MovieScene::GGlobalPlayerUpdateFlags.IsValidIndex(InUniqueIndex));
	UE::MovieScene::GGlobalPlayerUpdateFlags[InUniqueIndex] = bIsUpdating;
}

bool IMovieScenePlayer::IsEvaluating() const
{
	return UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex];
}

void IMovieScenePlayer::PopulateUpdateFlags(UE::MovieScene::ESequenceInstanceUpdateFlags& OutFlags)
{
	using namespace UE::MovieScene;

	OutFlags |= ESequenceInstanceUpdateFlags::NeedsPreEvaluation | ESequenceInstanceUpdateFlags::NeedsPostEvaluation;
}

void IMovieScenePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	UE::UniversalObjectLocator::FResolveParams ResolveParams(ResolutionContext);
	Sequence.LocateBoundObjects(InBindingId, ResolveParams, OutObjects);
}

TArrayView<TWeakObjectPtr<>> IMovieScenePlayer::FindBoundObjects(const FGuid& ObjectBindingID, FMovieSceneSequenceIDRef SequenceID)
{
	using namespace UE::MovieScene;

	if (TSharedPtr<const FSharedPlaybackState> SharedPlaybackState = FindSharedPlaybackState())
	{
		return State.FindBoundObjects(ObjectBindingID, SequenceID, SharedPlaybackState.ToSharedRef());
	}

	return TArrayView<TWeakObjectPtr<>>();
}

void IMovieScenePlayer::InvalidateCachedData()
{
	FMovieSceneRootEvaluationTemplateInstance& Template = GetEvaluationTemplate();

	UE::MovieScene::FSequenceInstance* RootInstance = Template.FindInstance(MovieSceneSequenceID::Root);
	if (RootInstance)
	{
		RootInstance->InvalidateCachedData();
	}
}

TSharedPtr<UE::MovieScene::FSharedPlaybackState> IMovieScenePlayer::FindSharedPlaybackState()
{
	const UE::MovieScene::FSequenceInstance* RootInstance = GetEvaluationTemplate().GetRootInstance();
	if (RootInstance)
	{
		return RootInstance->GetSharedPlaybackState();
	}
	return nullptr;
}

TSharedRef<UE::MovieScene::FSharedPlaybackState> IMovieScenePlayer::GetSharedPlaybackState()
{
	const UE::MovieScene::FSequenceInstance* RootInstance = GetEvaluationTemplate().GetRootInstance();
	check(RootInstance);
	return RootInstance->GetSharedPlaybackState();
}

void IMovieScenePlayer::ResetDirectorInstances()
{
	GetEvaluationTemplate().ResetDirectorInstances();
}

UObject* IMovieScenePlayer::GetOrCreateDirectorInstance(TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState, FMovieSceneSequenceIDRef SequenceID)
{
	return GetEvaluationTemplate().GetOrCreateDirectorInstance(SequenceID, *this);
}

void IMovieScenePlayer::InitializeRootInstance(TSharedRef<UE::MovieScene::FSharedPlaybackState> NewSharedPlaybackState)
{
	using namespace UE::MovieScene;

	NewSharedPlaybackState->AddCapability<FPlayerIndexPlaybackCapability>(UniqueIndex);
	NewSharedPlaybackState->AddCapabilityRaw(&State);
	NewSharedPlaybackState->AddCapabilityRaw(&GetSpawnRegister());
	NewSharedPlaybackState->AddCapabilityRaw((IObjectBindingNotifyPlaybackCapability*)this);
	NewSharedPlaybackState->AddCapabilityRaw((IStaticBindingOverridesPlaybackCapability*)this);
	NewSharedPlaybackState->AddCapabilityRaw((ISequenceDirectorPlaybackCapability*)this);

	if (IMovieScenePlaybackClient* PlaybackClient = GetPlaybackClient())
	{
		NewSharedPlaybackState->AddCapabilityRaw(PlaybackClient);
	}

	FInstanceRegistry* InstanceRegistry = NewSharedPlaybackState->GetLinker()->GetInstanceRegistry();
	FSequenceInstance& RootInstance = InstanceRegistry->MutateInstance(NewSharedPlaybackState->GetRootInstanceHandle());
	RootInstance.Initialize();
}

