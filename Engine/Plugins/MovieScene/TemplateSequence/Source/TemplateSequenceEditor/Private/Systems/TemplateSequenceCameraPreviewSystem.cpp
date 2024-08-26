// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/TemplateSequenceCameraPreviewSystem.h"

#include "Camera/CameraActor.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationExtension.h"
#include "EntitySystem/MovieSceneBlenderSystemTypes.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "LevelEditorViewport.h"
#include "Misc/TemplateSequenceEditorSettings.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneTracksComponentTypes.h"
#include "TemplateSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TemplateSequenceCameraPreviewSystem)

namespace UE::MovieScene
{

TEntitySystemLinkerExtensionID<FEditorViewportLinkerExtension> FEditorViewportLinkerExtension::GetExtensionID()
{
	static TEntitySystemLinkerExtensionID<FEditorViewportLinkerExtension> ID = UMovieSceneEntitySystemLinker::RegisterExtension<FEditorViewportLinkerExtension>();
	return ID;
}

TSharedPtr<FEditorViewportLinkerExtension> FEditorViewportLinkerExtension::GetOrCreateExtension(UMovieSceneEntitySystemLinker* Linker)
{
	if (FEditorViewportLinkerExtension* ViewportExtension = Linker->FindExtension<FEditorViewportLinkerExtension>())
	{
		return ViewportExtension->AsShared();
	}

	TSharedPtr<FEditorViewportLinkerExtension> NewViewportExtension = MakeShared<FEditorViewportLinkerExtension>(Linker);
	Linker->AddExtension(NewViewportExtension.Get());
	return NewViewportExtension;
}

FEditorViewportLinkerExtension::FEditorViewportLinkerExtension(UMovieSceneEntitySystemLinker* Linker)
	: TSharedEntitySystemLinkerExtension(Linker)
{
}

struct FEvaluateViewportTransform
{
	TOptional<FTransform> ViewportTransform;
	const FInstanceRegistry* InstanceRegistry;
	TArray<FMovieSceneEntityID> Entities;

	FEvaluateViewportTransform(const FInstanceRegistry* InInstanceRegistry, const TArray<FMovieSceneEntityID> InEntities)
		: InstanceRegistry(InInstanceRegistry)
		, Entities(InEntities)
	{}

	void PreTask()
	{
		ViewportTransform.Reset();
		if (GEditor)
		{
			for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
			{
				if (ViewportClient && 
						ViewportClient->IsPerspective() && 
						ViewportClient->GetViewMode() != VMI_Unknown &&
						ViewportClient->AllowsCinematicControl())
				{
					ViewportTransform.Emplace(FTransform::Identity);
					ViewportTransform->SetLocation(ViewportClient->GetViewLocation());
					ViewportTransform->SetRotation(ViewportClient->GetViewRotation().Quaternion());
					break;
				}
			}
		}
	}

	void ForEachAllocation(
			const FEntityAllocation* Allocation, 
			FReadEntityIDs EntityIDs,
			TWrite<FIntermediate3DTransform> InitialTransforms) const
	{
		if (!ViewportTransform.IsSet())
		{
			return;
		}

		const int32 Num = Allocation->Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FMovieSceneEntityID EntityID = EntityIDs[Index];
			if (Entities.Contains(EntityID))
			{
				FIntermediate3DTransform& InitialTransform = InitialTransforms[Index];
				InitialTransform = FIntermediate3DTransform(
						ViewportTransform->GetLocation(),
						ViewportTransform->GetRotation().Rotator(),
						ViewportTransform->GetScale3D());
			}
		}
	}
};

}

bool UTemplateSequenceCameraPreviewSystem::bEnableNextFrame = false;

void UTemplateSequenceCameraPreviewSystem::EnableNextFrame()
{
	bEnableNextFrame = true;
}

UTemplateSequenceCameraPreviewSystem::UTemplateSequenceCameraPreviewSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	Phase = ESystemPhase::Evaluation;
}

bool UTemplateSequenceCameraPreviewSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	return bEnableNextFrame;
}

void UTemplateSequenceCameraPreviewSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	// Don't actually run if we don't really want cameras to be additive to viewports.
	const UTemplateSequenceEditorSettings* Settings = GetDefault<UTemplateSequenceEditorSettings>();
	if (!Settings->bCameraInitiallyAdditiveToViewport)
	{
		return;
	}

	if (!bEnableNextFrame)
	{
		return;
	}

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	// Find all the object/component binding IDs that relate to camera actors.
	TArray<FGuid> CameraBindingIDs;
	for (auto It = InstanceRegistry->GetSparseInstances().CreateConstIterator(); It; ++It)
	{
		const FSequenceInstance& Instance = (*It);
		if (!Instance.IsRootSequence())
		{
			continue;
		}

		TSharedRef<FSharedPlaybackState> SharedPlaybackState = Instance.GetSharedPlaybackState();
		UTemplateSequence* RootSequence = Cast<UTemplateSequence>(SharedPlaybackState->GetRootSequence());
		if (!RootSequence)
		{
			continue;
		}

		const UObject* RootObjectTemplate = RootSequence->GetRootObjectSpawnableTemplate();
		if (!RootObjectTemplate || !RootObjectTemplate->IsA<ACameraActor>())
		{
			continue;
		}

		UMovieScene* MovieScene = RootSequence->GetMovieScene();
		if (!MovieScene || MovieScene->GetSpawnableCount() == 0)
		{
			continue;
		}

		const FMovieSceneSpawnable& FirstSpawnable = MovieScene->GetSpawnable(0);
		CameraBindingIDs.Add(FirstSpawnable.GetGuid());
	}

	// Now look for scene component binding entities that were imported from these object bindings.
	TArray<FMovieSceneEntityID> ParentsOfEntitiesToTag;

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->GenericObjectBinding)
	.FilterAll({ BuiltInComponents->Tags.ImportedEntity })
	.Iterate_PerEntity(&Linker->EntityManager,
			[&CameraBindingIDs, &ParentsOfEntitiesToTag](FMovieSceneEntityID EntityID, FGuid ObjectBindingID)
			{
				if (CameraBindingIDs.Contains(ObjectBindingID))
				{
					ParentsOfEntitiesToTag.Add(EntityID);
				}
			});

	// Next, look for transform entities generated from the ones we just found. We are looking for
	// the entities that hold the initial value of the camera transform. There are two situations:
	//
	// 1) They are non-blending entities that handle everything by themselves. Tag those with our
	//    custom tag.
	//
	// 2) They are part of a blending channel's inputs. In that case, we need to find the blending
	//	  channel output entity, because that's where the initial value is held.
	//
	TArray<FMovieSceneEntityID> EntitiesToTag;

	// Situation 1
	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->ParentEntity)
	.FilterAll({
			TrackComponents->ComponentTransform.PropertyTag,
			TrackComponents->ComponentTransform.InitialValue })
	.FilterNone({ BuiltInComponents->BlendChannelInput })
	.Iterate_PerEntity(&Linker->EntityManager, 
			[&ParentsOfEntitiesToTag, &EntitiesToTag](
				FMovieSceneEntityID EntityID, FMovieSceneEntityID ParentEntityID)
			{
				if (ParentsOfEntitiesToTag.Contains(ParentEntityID))
				{
					EntitiesToTag.Add(EntityID);
				}
			});

	// Situation 2
	TArray<FMovieSceneBlendChannelID> BlendChannelsToTag;

	FEntityTaskBuilder()
	.Read(BuiltInComponents->ParentEntity)
	.Read(BuiltInComponents->BlendChannelInput)
	.Iterate_PerEntity(&Linker->EntityManager, 
			[&ParentsOfEntitiesToTag, &BlendChannelsToTag](
				FMovieSceneEntityID ParentEntityID, FMovieSceneBlendChannelID BlendChannelID)
			{
				if (ParentsOfEntitiesToTag.Contains(ParentEntityID))
				{
					BlendChannelsToTag.Add(BlendChannelID);
				}
			});

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->BlendChannelOutput)
	.FilterAll({ 
			TrackComponents->ComponentTransform.PropertyTag,
			TrackComponents->ComponentTransform.InitialValue })
	.Iterate_PerEntity(&Linker->EntityManager,
			[&BlendChannelsToTag, &EntitiesToTag](FMovieSceneEntityID EntityID, FMovieSceneBlendChannelID BlendChannelID)
			{
				 if (BlendChannelsToTag.Contains(BlendChannelID))
				 {
					EntitiesToTag.Add(EntityID);
				 }
			});

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Write(TrackComponents->ComponentTransform.InitialValue)
	.FilterAll({ TrackComponents->ComponentTransform.PropertyTag })
	.Dispatch_PerAllocation<FEvaluateViewportTransform>(
			&Linker->EntityManager, InPrerequisites, &Subsequents, InstanceRegistry, EntitiesToTag);

	if (!Linker->FindExtension<IInterrogationExtension>())
	{
		bEnableNextFrame = false;
	}
}

