// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_Steering.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeFunctionRef.h"
#include "Animation/AnimRootMotionProvider.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimTrace.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_Steering)

void FAnimNode_Steering::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	if (USkeletalMeshComponent* SkelMeshComponent = Context.AnimInstanceProxy->GetSkelMeshComponent())
	{
		RootBoneTransform = SkelMeshComponent->GetBoneTransform(0);
	}
	else
	{
		RootBoneTransform = Context.AnimInstanceProxy->GetComponentTransform();
	}
}

void FAnimNode_Steering::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
}

void FAnimNode_Steering::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	if (Alpha > 0.0f)
	{
		const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();
		ensureMsgf(RootMotionProvider, TEXT("Steering expected a valid root motion delta provider interface."));

		if (RootMotionProvider)
		{
			FTransform RootMotionTransformDelta = FTransform::Identity;
			if (RootMotionProvider->ExtractRootMotion(Output.CustomAttributes, RootMotionTransformDelta))
			{
				const float DeltaSeconds = Output.AnimInstanceProxy->GetDeltaSeconds();

				FQuat RootBoneRotation = RootBoneTransform.GetRotation();

				UE_VLOG_ARROW(Output.AnimInstanceProxy->GetAnimInstanceObject(), "Steering", Display,
					RootBoneTransform.GetLocation(),
					RootBoneTransform.GetLocation()  + RootBoneRotation.GetForwardVector() * 100,
					FColor::Red, TEXT(""));
					
				UE_VLOG_ARROW(Output.AnimInstanceProxy->GetAnimInstanceObject(), "Steering", Display,
					RootBoneTransform.GetLocation(),
					RootBoneTransform.GetLocation()  + TargetOrientation.GetForwardVector() * 100,
					FColor::Green, TEXT(""));

				FQuat Delta =  RootBoneRotation.Inverse() * TargetOrientation;


				if (TargetTime > 0)
				{
					if (UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(CurrentAnimAsset))
					{
						FTransform RootMotionDelta = AnimSequence->ExtractRootMotion(CurrentAnimAssetTime, TargetTime, true);
						FQuat RootMotionRotation = RootMotionDelta.GetRotation();

						FRotator RootMotionRot(RootMotionRotation);
						
						if (fabs(RootMotionRot.Yaw) > RootMotionThreshold)
						{
							UE_VLOG_ARROW(Output.AnimInstanceProxy->GetAnimInstanceObject(), "Steering", Display,
								RootBoneTransform.GetLocation(),
								RootBoneTransform.GetLocation()  + (RootMotionRotation * RootBoneRotation).GetForwardVector() * 100,
								FColor::Blue, TEXT(""));

							FRotator DeltaRot(Delta);
							
							float Ratio =  (TargetTime / DeltaSeconds) * DeltaRot.Yaw / RootMotionRot.Yaw ;

							FRotator RootMotionFrameRot(RootMotionTransformDelta.GetRotation());

							RootMotionFrameRot.Yaw *= Ratio;
							
							RootMotionTransformDelta.SetRotation(FQuat(RootMotionFrameRot));
							RootMotionProvider->OverrideRootMotion(RootMotionTransformDelta, Output.CustomAttributes);
							
							return;
						}
					}
					
					Delta = FQuat::Slerp(FQuat::Identity, Delta,  DeltaSeconds/TargetTime);
				}

				RootMotionTransformDelta.SetRotation(FQuat::Slerp(RootMotionTransformDelta.GetRotation(), Delta, Alpha));
				RootMotionProvider->OverrideRootMotion(RootMotionTransformDelta, Output.CustomAttributes);
			}
		}
	}
}