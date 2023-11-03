// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneSequenceUpdaters.h"
#include "EntitySystem/MovieSceneSharedPlaybackState.h"

#include "Compilation/MovieSceneCompiledVolatilityManager.h"
#include "Compilation/MovieSceneCompiledDataManager.h"

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/Instances/MovieSceneTrackEvaluator.h"
#include "Evaluation/MovieSceneRootOverridePath.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"

#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieSceneTimeHelpers.h"

#include "Algo/IndexOf.h"

namespace UE
{
namespace MovieScene
{


DECLARE_CYCLE_STAT(TEXT("Sequence Instance Update"), MovieSceneEval_SequenceInstanceUpdate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("[External] Sequence Instance Post-Update"), MovieSceneEval_SequenceInstancePostUpdate, STATGROUP_MovieSceneEval);


void PurgeStaleTrackTemplates(UMovieSceneCompiledDataManager* CompiledDataManager, FMovieSceneCompiledDataID CompiledDataID)
{
	FMovieSceneEvaluationTemplate* EvalTemplate = const_cast<FMovieSceneEvaluationTemplate*>(CompiledDataManager->FindTrackTemplate(CompiledDataID));
	if (EvalTemplate)
	{
		EvalTemplate->PurgeStaleTracks();
	}

	// Do the same for all subsequences
	const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(CompiledDataID);
	if (Hierarchy)
	{
		for (const TTuple<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& Pair : Hierarchy->AllSubSequenceData())
		{
			UMovieSceneSequence* SubSequence = Pair.Value.GetLoadedSequence();
			if (!SubSequence)
			{
				continue;
			}
			FMovieSceneCompiledDataID SubCompiledDataID = CompiledDataManager->FindDataID(SubSequence);
			if (!SubCompiledDataID.IsValid())
			{
				continue;
			}

			FMovieSceneEvaluationTemplate* SubEvalTemplate = const_cast<FMovieSceneEvaluationTemplate*>(CompiledDataManager->FindTrackTemplate(SubCompiledDataID));
			if (SubEvalTemplate)
			{
				SubEvalTemplate->PurgeStaleTracks();
			}
		}
	}
}


FSequenceInstance::FSequenceInstance(TSharedRef<FSharedPlaybackState> PlaybackState, FRootInstanceHandle InInstanceHandle)
	: SharedPlaybackState(PlaybackState)
	, SequenceID(MovieSceneSequenceID::Root)
	, RootOverrideSequenceID(MovieSceneSequenceID::Root)
	, InstanceHandle(InInstanceHandle)
	, RootInstanceHandle(InInstanceHandle)
{
	UpdateFlags = ESequenceInstanceUpdateFlags::None;

	// Root instances always start in a finished state in order to ensure that 'Start'
	// is called correctly for the top level instance. This is subtly different from
	// bHasEverUpdated since a sequence instance can be Finished and restarted multiple times
	bFinished = true;
	bHasEverUpdated = false;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	UMovieSceneSequence* RootSequence = PlaybackState->GetRootSequence();
	RootSequenceName = RootSequence->GetPathName();
#endif
}

FSequenceInstance::FSequenceInstance(TSharedRef<FSharedPlaybackState> PlaybackState, FInstanceHandle InInstanceHandle, FInstanceHandle InParentInstanceHandle, FRootInstanceHandle InRootInstanceHandle, FMovieSceneSequenceID InSequenceID)
	: SharedPlaybackState(PlaybackState)
	, SequenceID(InSequenceID)
	, RootOverrideSequenceID(MovieSceneSequenceID::Invalid)
	, InstanceHandle(InInstanceHandle)
	, ParentInstanceHandle(InParentInstanceHandle)
	, RootInstanceHandle(InRootInstanceHandle)
{
	UpdateFlags = ESequenceInstanceUpdateFlags::None;

	// Sub Sequence instances always start in a non-finished state because they will only ever
	// be created if they are active, and the Start/Update/Finish loop does not apply to sub-instances
	bFinished = false;
	bHasEverUpdated = false;
}

void FSequenceInstance::Initialize(IMovieScenePlayer* Player)
{
	if (!Player)
	{
		return;
	}

	// Initialize playback capabilities if this is the root sequence.
	if (IsRootSequence())
	{
		SharedPlaybackState->AddCapability<FPlayerIndexPlaybackCapability>(Player->GetUniqueIndex());
	}

	UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
	if (ensure(Linker))
	{
		FMovieSceneObjectCache& ObjectCache = Player->State.GetObjectCache(SequenceID);
		OnInvalidateObjectBindingHandle = ObjectCache.OnBindingInvalidated.AddUObject(Linker, &UMovieSceneEntitySystemLinker::InvalidateObjectBinding, InstanceHandle);
	}

	InvalidateCachedData();
}

FSequenceInstance::~FSequenceInstance()
{}

FSequenceInstance::FSequenceInstance(FSequenceInstance&&) = default;

FSequenceInstance& FSequenceInstance::operator=(FSequenceInstance&&) = default;

IMovieScenePlayer* FSequenceInstance::GetPlayer() const
{
	using namespace UE::MovieScene;

	return FPlayerIndexPlaybackCapability::GetPlayer(SharedPlaybackState);
}

uint16 FSequenceInstance::GetPlayerIndex() const
{
	using namespace UE::MovieScene;

	return FPlayerIndexPlaybackCapability::GetPlayerIndex(SharedPlaybackState);
}

void FSequenceInstance::InitializeLegacyEvaluator()
{
	const FMovieSceneCompiledDataID RootCompiledDataID = SharedPlaybackState->GetRootCompiledDataID();
	UMovieSceneCompiledDataManager* CompiledDataManager = SharedPlaybackState->GetCompiledDataManager();
	const FMovieSceneCompiledDataEntry& CompiledEntry = CompiledDataManager->GetEntryRef(RootCompiledDataID);

	if (EnumHasAnyFlags(CompiledEntry.AccumulatedMask, EMovieSceneSequenceCompilerMask::EvaluationTemplate))
	{
		UpdateFlags |= ESequenceInstanceUpdateFlags::HasLegacyTemplates;

		if (!LegacyEvaluator)
		{
			UMovieSceneSequence* RootSequence = SharedPlaybackState->GetRootSequence();
			LegacyEvaluator = MakeUnique<FMovieSceneTrackEvaluator>(RootSequence, RootCompiledDataID, CompiledDataManager);
		}
	}
	else if (LegacyEvaluator)
	{
		IMovieScenePlayer* Player = GetPlayer();
		check(Player);

		LegacyEvaluator->Finish(*Player);
		LegacyEvaluator = nullptr;

		UpdateFlags &= ~ESequenceInstanceUpdateFlags::HasLegacyTemplates;
	}
}

void FSequenceInstance::InvalidateCachedData()
{
	// WARNING: this method is called from Initialize. If this is a root sequence, the Player is still
	// in the process of creating us, and is waiting for the call stack to return in order to set our
	// SharedPlaybackState and other relevant references on itself. So do NOT access anything through
	// the Player's RootEvaluationTemplateInstance here because it's most probably not initalized yet.

	Ledger.Invalidate();

	IMovieScenePlayer* Player = FPlayerIndexPlaybackCapability::GetPlayer(SharedPlaybackState);

	UpdateFlags = ESequenceInstanceUpdateFlags::None;

	const FMovieSceneCompiledDataID RootCompiledDataID = SharedPlaybackState->GetRootCompiledDataID();
	UMovieSceneCompiledDataManager* CompiledDataManager = SharedPlaybackState->GetCompiledDataManager();

	if (SequenceID == MovieSceneSequenceID::Root)
	{
		SharedPlaybackState->InvalidateCachedData();

		if (Player)
		{
			UMovieSceneSequence* RootSequence = SharedPlaybackState->GetRootSequence();
			Player->State.AssignSequence(SequenceID, *RootSequence, *Player);
		}

		// Try and recreate the volatility manager if this sequence is now volatile
		if (!VolatilityManager)
		{
			VolatilityManager = FCompiledDataVolatilityManager::Construct(SharedPlaybackState);
		}

		ISequenceUpdater::FactoryInstance(SequenceUpdater, CompiledDataManager, RootCompiledDataID);

		UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
		SequenceUpdater->InvalidateCachedData(Linker, RootInstanceHandle);
		SequenceUpdater->PopulateUpdateFlags(Linker, SharedPlaybackState, UpdateFlags);

		if (LegacyEvaluator)
		{
			LegacyEvaluator->InvalidateCachedData();
		}

		InitializeLegacyEvaluator();
	}
	else if (UMovieSceneSequence* SubSequence = SharedPlaybackState->GetSequence(SequenceID))
	{
		if (Player)
		{
			Player->State.AssignSequence(SequenceID, *SubSequence, *Player);
		}
	}
}

bool FSequenceInstance::ConditionalRecompile()
{
	if (VolatilityManager)
	{
		if (VolatilityManager->ConditionalRecompile())
		{
			InvalidateCachedData();
			return true;
		}
	}

	return false;
}

void FSequenceInstance::DissectContext(const FMovieSceneContext& InContext, TArray<TRange<FFrameTime>>& OutDissections)
{
	if (EnumHasAnyFlags(UpdateFlags, ESequenceInstanceUpdateFlags::NeedsDissection))
	{
		check(SequenceID == MovieSceneSequenceID::Root);
		UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
		SequenceUpdater->DissectContext(Linker, SharedPlaybackState, InContext, OutDissections);
	}
}

void FSequenceInstance::Start(const FMovieSceneContext& InContext)
{
	check(SequenceID == MovieSceneSequenceID::Root);

	bFinished = false;
	bHasEverUpdated = true;

	check(RootInstanceHandle == InstanceHandle);

	UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
	SequenceUpdater->Start(Linker, RootInstanceHandle, SharedPlaybackState, InContext);
}

void FSequenceInstance::Update(const FMovieSceneContext& InContext)
{
	SCOPE_CYCLE_COUNTER(MovieSceneEval_SequenceInstanceUpdate);
	SCOPE_CYCLE_UOBJECT(ContextScope, GetPlayer()->AsUObject());

	bHasEverUpdated = true;
	UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();

	if (bFinished)
	{
		Start(InContext);
	}

	check(RootInstanceHandle == InstanceHandle);

	Context = InContext;
	SequenceUpdater->Update(Linker, RootInstanceHandle, SharedPlaybackState, InContext);
}

bool FSequenceInstance::CanFinishImmediately() const
{
	if (SequenceUpdater)
	{
		check(RootInstanceHandle == InstanceHandle);

		UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
		return SequenceUpdater->CanFinishImmediately(Linker, RootInstanceHandle);
	}

	return true;
}

void FSequenceInstance::Finish()
{
	if (IsRootSequence() && !bHasEverUpdated)
	{
		return;
	}

	UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
	Linker->EntityManager.IncrementSystemSerial();
	bFinished = true;
	Ledger.UnlinkEverything(Linker);

	Ledger = FEntityLedger();

	IMovieScenePlayer* Player = GetPlayer();
	if (!ensure(Player))
	{
		return;
	}

	if (SequenceUpdater)
	{
		check(RootInstanceHandle == InstanceHandle);
		SequenceUpdater->Finish(Linker, RootInstanceHandle, SharedPlaybackState);
	}

	if (LegacyEvaluator)
	{
		LegacyEvaluator->Finish(*Player);
	}

	if (IsRootSequence())
	{
		FMovieSceneSpawnRegister& SpawnRegister = Player->GetSpawnRegister();
		SpawnRegister.ForgetExternallyOwnedSpawnedObjects(Player->State, *Player);
		SpawnRegister.CleanUp(*Player);

		if (Player->PreAnimatedState.IsCapturingGlobalPreAnimatedState())
		{
			Linker->PreAnimatedState.RestoreGlobalState(FRestoreStateParams{ Linker, RootInstanceHandle });
		}
	}
}

void FSequenceInstance::PreEvaluation()
{
	if (!EnumHasAnyFlags(UpdateFlags, ESequenceInstanceUpdateFlags::NeedsPreEvaluation))
	{
		return;
	}

	if (IsRootSequence())
	{
		IMovieScenePlayer* Player = GetPlayer();
		if (ensure(Player))
		{
			Player->PreEvaluation(Context);
		}
	}
}

void FSequenceInstance::RunLegacyTrackTemplates()
{
	if (LegacyEvaluator)
	{
		IMovieScenePlayer* Player = GetPlayer();
		if (ensure(Player))
		{
			if (bFinished)
			{
				LegacyEvaluator->Finish(*Player);
			}
			else
			{
				LegacyEvaluator->Evaluate(Context, *Player, RootOverrideSequenceID);
			}
		}
	}
}

void FSequenceInstance::PostEvaluation()
{
	if (IsRootSequence() && EnumHasAnyFlags(UpdateFlags, ESequenceInstanceUpdateFlags::NeedsPostEvaluation))
	{
		IMovieScenePlayer* Player = GetPlayer();
		if (ensure(Player))
		{
			SCOPE_CYCLE_COUNTER(MovieSceneEval_SequenceInstancePostUpdate);


			// DANGER: This function is highly fragile due to the nature of IMovieScenePlayer::PostEvaluation
			//         being able to re-evaluate sequences. Ultimately this can lead to FSequenceInstances being
			//         created, destroyed, or reallocated. As such
			//
			//                  ***** the current this ptr can become invalid at any point ***** 
			//
			//         Any code which needs to run after PostEvaluate must cache any member variables it needs on
			//         the stack _before_ Player->PostEvaluation is called.


			// If this sequence is volatile and has legacy track templates, purge any stale track templates from the compiled data after evaluation
			const bool bShouldPurgeTemplates = VolatilityManager && LegacyEvaluator;

			UMovieSceneCompiledDataManager* LocalCompiledDataManager = bShouldPurgeTemplates ? Player->GetEvaluationTemplate().GetCompiledDataManager() : nullptr;
			FMovieSceneCompiledDataID       LocalCompiledDataID      = bShouldPurgeTemplates ? Player->GetEvaluationTemplate().GetCompiledDataID()      : FMovieSceneCompiledDataID();

			Player->PostEvaluation(Context);

			if (LocalCompiledDataManager)
			{
				PurgeStaleTrackTemplates(LocalCompiledDataManager, LocalCompiledDataID);
			}
		}
	}
}

void FSequenceInstance::DestroyImmediately()
{
	UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();

	if (!Ledger.IsEmpty())
	{
		UE_LOG(LogMovieSceneECS, Verbose, TEXT("Instance being destroyed without first having been finished by calling Finish()"));
		Ledger.UnlinkEverything(Linker, EUnlinkEverythingMode::CleanGarbage);
	}

	if (SequenceUpdater)
	{
		SequenceUpdater->Destroy(Linker);
	}
}

void FSequenceInstance::OverrideRootSequence(FMovieSceneSequenceID NewRootSequenceID)
{
	if (SequenceUpdater)
	{
		check(RootInstanceHandle == InstanceHandle);
		UMovieSceneEntitySystemLinker* Linker = SharedPlaybackState->GetLinker();
		SequenceUpdater->OverrideRootSequence(Linker, RootInstanceHandle, NewRootSequenceID);
	}

	RootOverrideSequenceID = NewRootSequenceID;
}

FInstanceHandle FSequenceInstance::FindSubInstance(FMovieSceneSequenceID SubSequenceID) const
{
	return SequenceUpdater ? SequenceUpdater->FindSubInstance(SubSequenceID) : FInstanceHandle();
}

FMovieSceneEntityID FSequenceInstance::FindEntity(UObject* Owner, uint32 EntityID) const
{
	return Ledger.FindImportedEntity(FMovieSceneEvaluationFieldEntityKey{ decltype(FMovieSceneEvaluationFieldEntityKey::EntityOwner)(Owner), EntityID });
}

void FSequenceInstance::FindEntities(UObject* Owner, TArray<FMovieSceneEntityID>& OutEntityIDs) const
{
	Ledger.FindImportedEntities(Owner, OutEntityIDs);
}

FSubSequencePath FSequenceInstance::GetSubSequencePath() const
{
	return FSubSequencePath(SequenceID, *GetPlayer());
}

bool FSequenceInstance::ConditionalRecompile(UMovieSceneEntitySystemLinker* Linker)
{
	return ConditionalRecompile();
}

void FSequenceInstance::DissectContext(UMovieSceneEntitySystemLinker* Linker, const FMovieSceneContext& InContext, TArray<TRange<FFrameTime>>& OutDissections)
{
	DissectContext(InContext, OutDissections);
}

void FSequenceInstance::Start(UMovieSceneEntitySystemLinker* Linker, const FMovieSceneContext& InContext)
{
	Start(InContext);
}

void FSequenceInstance::PreEvaluation(UMovieSceneEntitySystemLinker* Linker)
{
	PreEvaluation();
}

void FSequenceInstance::Update(UMovieSceneEntitySystemLinker* Linker, const FMovieSceneContext& InContext)
{
	Update(InContext);
}

bool FSequenceInstance::CanFinishImmediately(UMovieSceneEntitySystemLinker* Linker) const
{
	return CanFinishImmediately();
}

void FSequenceInstance::Finish(UMovieSceneEntitySystemLinker* Linker)
{
	Finish();
}

void FSequenceInstance::PostEvaluation(UMovieSceneEntitySystemLinker* Linker)
{
	PostEvaluation();
}

void FSequenceInstance::InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker)
{
	InvalidateCachedData();
}

void FSequenceInstance::DestroyImmediately(UMovieSceneEntitySystemLinker* Linker)
{
	DestroyImmediately();
}

void FSequenceInstance::OverrideRootSequence(UMovieSceneEntitySystemLinker* Linker, FMovieSceneSequenceID NewRootSequenceID)
{
	OverrideRootSequence(NewRootSequenceID);
}

} // namespace MovieScene
} // namespace UE
