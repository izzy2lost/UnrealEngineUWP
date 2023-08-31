// Copyright Epic Games, Inc. All Rights Reserved.


#if WITH_EDITOR

#include "FootstepAnimEventsModifier.h"
#include "Animation/AnimSequence.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimPose.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#define LOCTEXT_NAMESPACE "FootstepAnimEventsModifierBase"

UFootstepAnimEventsModifier::UFootstepAnimEventsModifier() : Super()
{
	SampleStep = 0.0125f;
	GroundThreshold = 0.95f;
	bShouldRemovePreExistingNotifiesOrSyncMarkers = false;
}

void UFootstepAnimEventsModifier::OnApply_Implementation(UAnimSequence* InAnimation)
{
	Super::OnApply_Implementation(InAnimation);

	if (InAnimation == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("FootstepAnimEventsModifierBase failed. Reason: Invalid Animation"));
		return;
	}
	
	// Disable root motion lock during application
	bIsRootMotionLocked = InAnimation->bForceRootLock;
	InAnimation->bForceRootLock = false;

	// Clean up or generate tracks if needed
	ValidateNotifyTracks(InAnimation);
	
	// Process animation
	{
		TArray<FFootSampleState> FootSampleStates;
		FootSampleStates.Init(FFootSampleState(), FootDefinitions.Num());
		
		const float SequenceLength = InAnimation->GetPlayLength();
		const int SampleNum = FMath::TruncToInt(SequenceLength / SampleStep);

		// Get ground levels for each foot definition
		for (int i = 0; i < SampleNum; ++i)
		{
			const float SampleTime = static_cast<float>(i) * SampleStep;
			FAnimPose AnimPose;
		
			// Query animation pose at the current sample time
			{
				FAnimPoseEvaluationOptions AnimPoseEvalOptions;
				AnimPoseEvalOptions.EvaluationType = EAnimDataEvalType::Raw;
			
				UAnimPoseExtensions::GetAnimPoseAtTime(InAnimation, SampleTime, AnimPoseEvalOptions, AnimPose);
			}

			// Find out ground level for each foot
			for (int CurrFootIdx = 0; CurrFootIdx < FootDefinitions.Num(); ++CurrFootIdx)
			{
				const FFootDefinition & FootDef = FootDefinitions[CurrFootIdx];
				FFootSampleState & FootState = FootSampleStates[CurrFootIdx];

				// Compute ground level
				FootState.GroundLevel = FMath::Min(FootState.GroundLevel, UAnimPoseExtensions::GetBonePose(AnimPose, FootDef.FootBoneName, EAnimPoseSpaces::World).GetLocation().Z);
			}
		}

		// Avoid creating a notify at the very start of a sequence if the foot bone is already within ground threshold 
		{
			FAnimPose AnimPose;
			
			FAnimPoseEvaluationOptions AnimPoseEvalOptions;
			AnimPoseEvalOptions.EvaluationType = EAnimDataEvalType::Raw;
			
			UAnimPoseExtensions::GetAnimPoseAtTime(InAnimation, 0.0f, AnimPoseEvalOptions, AnimPose);
			
			for (int CurrFootIdx = 0; CurrFootIdx < FootDefinitions.Num(); ++CurrFootIdx)
			{
				const FFootDefinition & FootDef = FootDefinitions[CurrFootIdx];
				FFootSampleState & FootState = FootSampleStates[CurrFootIdx];
				
				FootState.bWasFootBoneInGround = FMath::Abs(FootState.GroundLevel - UAnimPoseExtensions::GetBonePose(AnimPose, FootDef.FootBoneName, EAnimPoseSpaces::World).GetLocation().Z) < GroundThreshold;
			}
		}
		
		// Generate animation events
		for (int i = 0; i < SampleNum; ++i)
		{
			const float SampleTime = static_cast<float>(i) * SampleStep;

			FAnimPose AnimPose;
			FAnimPose FutureAnimPose;
			
			// Query animation pose at the current sample time
			{
				FAnimPoseEvaluationOptions AnimPoseEvalOptions;
				AnimPoseEvalOptions.EvaluationType = EAnimDataEvalType::Raw;
				
				UAnimPoseExtensions::GetAnimPoseAtTime(InAnimation, SampleTime, AnimPoseEvalOptions, AnimPose);
				UAnimPoseExtensions::GetAnimPoseAtTime(InAnimation, FMath::Clamp(SampleTime + SampleStep, 0.0f, SequenceLength), AnimPoseEvalOptions, FutureAnimPose);
			}

			// Process all foot definitions
			for (int CurrFootIdx = 0; CurrFootIdx < FootDefinitions.Num(); ++CurrFootIdx)
			{
				const FFootDefinition & FootDef = FootDefinitions[CurrFootIdx];
				FFootSampleState & FootState = FootSampleStates[CurrFootIdx];
				
				// Update current sample state
				{
					// Get reference bone translation
					const FTransform ReferenceBoneTransform = UAnimPoseExtensions::GetBonePose(AnimPose, FootDef.ReferenceBoneName, EAnimPoseSpaces::World);
					const FTransform FutureReferenceBoneTransform = UAnimPoseExtensions::GetBonePose(FutureAnimPose, FootDef.ReferenceBoneName, EAnimPoseSpaces::World);
					const FVector ReferenceBoneTranslation = FutureReferenceBoneTransform.GetLocation() - ReferenceBoneTransform.GetLocation();

					// Get foot bone direction with respect to reference bone
					const FTransform FootBoneTransform = UAnimPoseExtensions::GetBonePose(AnimPose, FootDef.FootBoneName, EAnimPoseSpaces::World);
					const FVector ReferenceBoneToFootBoneVector = FootBoneTransform.GetLocation() - ReferenceBoneTransform.GetLocation();

					FootState.RefBoneTranslationDotRefBoneToFootBoneVec = FVector::DotProduct(ReferenceBoneTranslation, ReferenceBoneToFootBoneVector);
					FootState.bIsFootBoneInGround = FMath::Abs(FootState.GroundLevel - FootBoneTransform.GetLocation().Z) < GroundThreshold;
				}
				
				// Generate sync markers
				if (FootDef.bShouldGenerateSyncMarkers && CanWePlaceEventAtSample(FootState, FootDef.SyncMarkerDetectionTechnique))
				{
					UAnimationBlueprintLibrary::AddAnimationSyncMarker(InAnimation, FootDef.SyncMarkerName, SampleTime, FootDef.SyncMarkerTrackName);
				}

				// Generate foot step fx notifies
				if (FootDef.bShouldGenerateNotifies && CanWePlaceEventAtSample(FootState, FootDef.FootstepNotifyDetectionTechnique))
				{
					UAnimationBlueprintLibrary::AddAnimationNotifyEvent(InAnimation, FootDef.FootstepNotifyTrackName, SampleTime, FootDef.FootstepNotify);
				}					

				// Update foot state
				FootState.bWasFootBoneInGround = FootState.bIsFootBoneInGround;
				FootState.PrevRefBoneTranslationDotRefBoneToFootBoneVec = FootState.RefBoneTranslationDotRefBoneToFootBoneVec;
			}
		}
	}

	// Revert modifications to animation state
	InAnimation->bForceRootLock = bIsRootMotionLocked;
}

void UFootstepAnimEventsModifier::OnRevert_Implementation(UAnimSequence* InAnimation)
{
	Super::OnRevert_Implementation(InAnimation);

	// Delete any generate tracks.
	for (const FName GeneratedNotifyTrack : GeneratedNotifyTracks)
	{
		UAnimationBlueprintLibrary::RemoveAnimationNotifyTrack(InAnimation, GeneratedNotifyTrack);
	}
	
	// Delete any generated anim events.
	for (const FName ProcessedNotifyTrack : ProcessedNotifyTracks)
	{
		const bool IsNotifyTrackValid = UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(InAnimation, ProcessedNotifyTrack);

		if (IsNotifyTrackValid)
		{
			for (const FFootDefinition& FootDef : FootDefinitions)
			{
				if (FootDef.bShouldGenerateSyncMarkers && FootDef.SyncMarkerTrackName == ProcessedNotifyTrack)
				{
					UAnimationBlueprintLibrary::RemoveAnimationSyncMarkersByName(InAnimation, FootDef.SyncMarkerName);
				}

				if (FootDef.bShouldGenerateNotifies && FootDef.FootstepNotifyTrackName == ProcessedNotifyTrack)
				{
					if (FootDef.FootstepNotify)
					{
						UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByName(InAnimation, FName(Cast<UAnimNotify>(FootDef.FootstepNotify->GetDefaultObject())->GetNotifyName()));
					}
					else
					{
						UE_LOG(LogAnimation, Error, TEXT("FootstepAnimEventsModifierBase failed. Reason: Invalid UAnimNotify class"));
					}
				}
			}
		}
	}
	
	GeneratedNotifyTracks.Reset();
	ProcessedNotifyTracks.Reset();
}

void UFootstepAnimEventsModifier::ValidateNotifyTracks(UAnimSequence* InAnimation)
{
	check(InAnimation != nullptr)

	GatherNotifyTracksInfo(InAnimation);
	
	for (const FFootDefinition & FootDef : FootDefinitions)
	{
		if (FootDef.bShouldGenerateSyncMarkers)
		{
			PrepareNotifyTrack(InAnimation, FootDef.SyncMarkerTrackName);
		}
		
		if (FootDef.bShouldGenerateNotifies)
		{
			PrepareNotifyTrack(InAnimation, FootDef.FootstepNotifyTrackName);
		}
	}
}

void UFootstepAnimEventsModifier::GatherNotifyTracksInfo(const UAnimSequence* InAnimation)
{
	for (const FFootDefinition& FootDef : FootDefinitions)
	{
		// Determine tracks that will be generated
		{
			const bool bDoesRequestedSyncTrackAlreadyExist = FootDef.bShouldGenerateSyncMarkers && UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(InAnimation, FootDef.SyncMarkerTrackName);
			const bool bDoesRequestedNotifyTrackAlreadyExist = FootDef.bShouldGenerateNotifies && UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(InAnimation, FootDef.FootstepNotifyTrackName);
	
			if (!bDoesRequestedSyncTrackAlreadyExist)
			{
				GeneratedNotifyTracks.Add(FootDef.SyncMarkerTrackName);
			}

			if (!bDoesRequestedNotifyTrackAlreadyExist)
			{
				GeneratedNotifyTracks.Add(FootDef.FootstepNotifyTrackName);
			}
		}

		// Determine tracks that will be modified
		ProcessedNotifyTracks.Add(FootDef.SyncMarkerTrackName);
		ProcessedNotifyTracks.Add(FootDef.FootstepNotifyTrackName);
	}
}

void UFootstepAnimEventsModifier::PrepareNotifyTrack(UAnimSequence* InAnimation, FName InRequestedNotifyTrackName)
{
	const bool bDoesTrackNameAlreadyExist = UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(InAnimation, InRequestedNotifyTrackName);
	
	if (!bDoesTrackNameAlreadyExist)
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(InAnimation, InRequestedNotifyTrackName, FLinearColor::MakeRandomColor());
	}
	else if (bShouldRemovePreExistingNotifiesOrSyncMarkers)
	{
		UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByTrack(InAnimation, InRequestedNotifyTrackName);
		UAnimationBlueprintLibrary::RemoveAnimationSyncMarkersByTrack(InAnimation, InRequestedNotifyTrackName);
	}
}

bool UFootstepAnimEventsModifier::CanWePlaceEventAtSample(const FFootSampleState& InTestFootSampleState, EDetectionTechnique DetectionTechnique)
{
	switch(DetectionTechnique)
	{
		case EDetectionTechnique::PassThroughReferenceBone: return InTestFootSampleState.RefBoneTranslationDotRefBoneToFootBoneVec > 0.0f && InTestFootSampleState.PrevRefBoneTranslationDotRefBoneToFootBoneVec < 0.0f;
		case EDetectionTechnique::FootBoneReachesGround: return !InTestFootSampleState.bWasFootBoneInGround && InTestFootSampleState.bIsFootBoneInGround;
		default: return false;
	}
}
#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
