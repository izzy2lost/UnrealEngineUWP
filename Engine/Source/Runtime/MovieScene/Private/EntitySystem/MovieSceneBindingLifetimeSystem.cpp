// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneBindingLifetimeSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/BuiltInComponentTypes.h"

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"

#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieSceneEntityRange.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"

#include "MovieScene.h"
#include "MovieSceneExecutionToken.h"
#include "IMovieScenePlaybackClient.h"
#include "EntitySystem/MovieSceneSpawnablesSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingLifetimeSystem)
#define LOCTEXT_NAMESPACE "MovieSceneBindingLifetimeSystem"

namespace UE
{
	namespace MovieScene
	{
		const FMovieSceneAnimTypeID BindingLifetimeAnimTypeID = FMovieSceneAnimTypeID::Unique();
	}
}


UMovieSceneBindingLifetimeSystem::UMovieSceneBindingLifetimeSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	Phase = UE::MovieScene::ESystemPhase::Spawn;
	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();
	RelevantComponent = BuiltInComponentTypes->BindingLifetime;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(GetClass(), UMovieSceneSpawnablesSystem::StaticClass());
	}
}

FMovieSceneAnimTypeID UMovieSceneBindingLifetimeSystem::GetAnimTypeID()
{
	return UE::MovieScene::BindingLifetimeAnimTypeID;
}

void UMovieSceneBindingLifetimeSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	if (!Linker->EntityManager.Contains(FEntityComponentFilter().Any({ BuiltInComponents->Tags.NeedsLink, BuiltInComponents->Tags.NeedsUnlink })))
	{
		return;
	}

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	bool bLink = false;
	auto SetBindingActivation = [InstanceRegistry, &bLink](FMovieSceneEntityID EntityID, FInstanceHandle InstanceHandle, const FMovieSceneBindingLifetimeComponentData& BindingLifetime)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneSequenceID SequenceID = SequenceInstance.GetSequenceID();
		IMovieScenePlayer* Player = SequenceInstance.GetPlayer();
		if (Player)
		{
			// For now we use the linking/unlinking of the inactive ranges to set the binding activations
			if (BindingLifetime.BindingLifetimeState == EMovieSceneBindingLifetimeState::InActive)
			{
				Player->State.SetBindingActivation(BindingLifetime.BindingGuid, SequenceID, !bLink);
			}
		}
	};

	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();

	// Unlink stale bindinglifetime entities
	FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponentTypes->BindingLifetime)
		.FilterAll({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerEntity(&Linker->EntityManager, SetBindingActivation);

	// Link new bindinglifetime entities
	bLink = true;
	FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponentTypes->BindingLifetime)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerEntity(&Linker->EntityManager, SetBindingActivation);
}

#undef LOCTEXT_NAMESPACE

