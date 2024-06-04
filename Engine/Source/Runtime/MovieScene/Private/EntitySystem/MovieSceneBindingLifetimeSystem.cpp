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
#include "MovieSceneBindingEventReceiverInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingLifetimeSystem)
#define LOCTEXT_NAMESPACE "MovieSceneBindingLifetimeSystem"


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

void UMovieSceneBindingLifetimeSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	if (!Linker->EntityManager.Contains(FEntityComponentFilter().Any({ BuiltInComponents->Tags.NeedsLink, BuiltInComponents->Tags.NeedsUnlink })))
	{
		return;
	}

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();
	FPreAnimatedStateExtension& PreAnimatedState = Linker->PreAnimatedState;

	bool bLink = false;
	auto SetBindingActivation = [InstanceRegistry, &PreAnimatedState, &bLink](FMovieSceneEntityID EntityID, FInstanceHandle InstanceHandle, const FMovieSceneBindingLifetimeComponentData& BindingLifetime)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneSequenceID SequenceID = SequenceInstance.GetSequenceID();
		TSharedRef<const FSharedPlaybackState> SharedPlaybackState = SequenceInstance.GetSharedPlaybackState();
		FMovieSceneEvaluationState* EvaluationState = SharedPlaybackState->FindCapability<FMovieSceneEvaluationState>();
		IMovieScenePlayer* Player = FPlayerIndexPlaybackCapability::GetPlayer(SharedPlaybackState);
		if (EvaluationState)
		{
			// For now we use the linking/unlinking of the inactive ranges to set the binding activations
			if (BindingLifetime.BindingLifetimeState == EMovieSceneBindingLifetimeState::Inactive)
			{
				EvaluationState->SetBindingActivation(BindingLifetime.BindingGuid, SequenceID, !bLink);
			}
			else
			{
				TArrayView<TWeakObjectPtr<>> BoundObjects = EvaluationState->FindBoundObjects(BindingLifetime.BindingGuid, SequenceID, SharedPlaybackState);
				for (TWeakObjectPtr<> WeakBoundObject : BoundObjects)
				{
					if (UObject* BoundObject = WeakBoundObject.Get())
					{
						if (BoundObject->Implements<UMovieSceneBindingEventReceiverInterface>())
						{
							TScriptInterface<IMovieSceneBindingEventReceiverInterface> BindingEventReceiver = BoundObject;
							if (BindingEventReceiver.GetObject() && Player)
							{
								FMovieSceneObjectBindingID BindingID = UE::MovieScene::FRelativeObjectBindingID(MovieSceneSequenceID::Root, SequenceID, BindingLifetime.BindingGuid, SharedPlaybackState);
								if (bLink)
								{
									IMovieSceneBindingEventReceiverInterface::Execute_OnObjectBoundBySequencer(BindingEventReceiver.GetObject(), Cast<UMovieSceneSequencePlayer>(Player->AsUObject()), BindingID);
								}
								else
								{
									IMovieSceneBindingEventReceiverInterface::Execute_OnObjectUnboundBySequencer(BindingEventReceiver.GetObject(), Cast<UMovieSceneSequencePlayer>(Player->AsUObject()), BindingID);
								}
							}
						}
					}
				}
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

