// Copyright Epic Games, Inc. All Rights Reserved.
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"

#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "RootMotionRemapOp"

#if WITH_EDITOR
void URootMotionGeneratorOp::OnAddedToStack(const UIKRetargeter* Asset)
{
	// get the root of the SOURCE skeleton
	if (const USkeletalMesh* Mesh = Asset->GetPreviewMesh(ERetargetSourceOrTarget::Source))
	{
		SourceRootBone = Mesh->GetRefSkeleton().GetBoneName(0);
	}
	
	// get the root of the TARGET skeleton
	if (const USkeletalMesh* Mesh = Asset->GetPreviewMesh(ERetargetSourceOrTarget::Target))
	{
		TargetRootBone = Mesh->GetRefSkeleton().GetBoneName(0);
	}

	// get the retarget root bone
	if (const UIKRigDefinition* IKRig = Asset->GetIKRig(ERetargetSourceOrTarget::Target))
	{
		TargetPelvisBone = IKRig->GetRetargetRoot();
	}
}
#endif

bool URootMotionGeneratorOp::Initialize(
	const UIKRetargetProcessor* Processor,
	const FRetargetSkeleton& SourceSkeleton,
	const FTargetSkeleton& TargetSkeleton,
	FIKRigLogger& Log)
{
	SourceRootIndex = SourceSkeleton.BoneNames.Find(SourceRootBone);
	bool bHasAllPrerequisites = true;
	if (SourceRootIndex == INDEX_NONE)
	{
		Log.LogWarning(FText::Format(LOCTEXT("MissingRootBone", "Root Motion Remap Op, missing source root bone {0}."), FText::FromName(SourceRootBone)));
		bHasAllPrerequisites = false;
	}
	
	TargetRootIndex = TargetSkeleton.BoneNames.Find(TargetRootBone);
	if (TargetRootIndex == INDEX_NONE)
	{
		Log.LogWarning(FText::Format(LOCTEXT("MissingRootBone", "Root Motion Remap Op, missing target root bone {0}."), FText::FromName(TargetRootBone)));
		bHasAllPrerequisites = false;
	}

	TargetPelvisIndex = TargetSkeleton.BoneNames.Find(TargetPelvisBone);
	if (TargetPelvisIndex == INDEX_NONE)
	{
		Log.LogWarning(FText::Format(LOCTEXT("MissingPelvisBone", "Root Motion Remap Op, missing target pelvis bone {0}."), FText::FromName(TargetPelvisBone)));
		bHasAllPrerequisites = false;
	}

#if WITH_EDITOR
	if (bHasAllPrerequisites)
	{
		Message = FText::Format(LOCTEXT("ReadyToRun", "Generating root motion on {0}."), FText::FromName(TargetRootBone));
	}
	else
	{
		Message = FText(LOCTEXT("MissingBonesWarning", "Bone(s) not found. See output log."));
	}
#endif

	// can't cache everything unless all prerequisites are met
	if (!bHasAllPrerequisites)
	{
		return false;
	}

	TargetPelvisInRefPose = TargetSkeleton.RetargetGlobalPose[TargetPelvisIndex];
	SourceRootInRefPose = SourceSkeleton.RetargetGlobalPose[SourceRootIndex];
	TargetRootInRefPose = TargetSkeleton.RetargetGlobalPose[TargetRootIndex];
	TargetPelvisRelativeToTargetRootRefPose = TargetRootInRefPose.GetRelativeTransform(TargetPelvisInRefPose);

	// generate list of non-retargeted child indices
	const int32 NumTargetBones = TargetSkeleton.BoneNames.Num();
	NonRetargetedChildrenOfRoot.Reset();
	for (int32 BoneIndex=1; BoneIndex<NumTargetBones; ++BoneIndex)
	{
		bool bHasNoRetargetedParent = true;
		
		int32 ParentIndex = BoneIndex;
		while(ParentIndex != INDEX_NONE)
		{
			if (TargetSkeleton.IsBoneRetargeted[ParentIndex])
			{
				bHasNoRetargetedParent = false;
				break;
			}
			ParentIndex = TargetSkeleton.ParentIndices[ParentIndex];
		}

		if (bHasNoRetargetedParent)
		{
			NonRetargetedChildrenOfRoot.Add(BoneIndex);
		}
	}
	
	return true;
}

void URootMotionGeneratorOp::Run(
	const UIKRetargetProcessor* Processor,
	const TArray<FTransform>& InSourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose)
{
	// generate a new transform for the target root bone
	FTransform NewRootTransform;
	
	// either generate motion from target pelvis, or copy from source root
	if (RootMotionSource == ERootMotionSource::GenerateFromTargetPelvis)
	{
		GenerateRootMotionFromTargetPelvis(NewRootTransform, InSourceGlobalPose, OutTargetGlobalPose);
	}
	else
	{
		CopyRootMotionFromSourceRoot(NewRootTransform, Processor, InSourceGlobalPose);
	}

	// optionally propagate delta to all non-retargeted children
	if (bPropagateToNonRetargetedChildren)
	{
		const FTransform Delta = OutTargetGlobalPose[TargetRootIndex].Inverse() * NewRootTransform;
		for (const int32 BoneIndex : NonRetargetedChildrenOfRoot)
		{
			OutTargetGlobalPose[BoneIndex] = OutTargetGlobalPose[BoneIndex] * Delta;
		}
	}

	// apply to the target root bone
	OutTargetGlobalPose[TargetRootIndex] = NewRootTransform * GlobalOffset;
}

void URootMotionGeneratorOp::GenerateRootMotionFromTargetPelvis(
	FTransform& OutRootTransform,
	const TArray<FTransform>& InSourceGlobalPose,
	const TArray<FTransform>& InTargetGlobalPose) const
{
	// In this case, we are generating root motion "from scratch"
	// using the target Pelvis bone as the source of the motion
		
	// calculate new root transform relative to current hips transform
	OutRootTransform = InTargetGlobalPose[TargetPelvisIndex];
	// optionally remove all rotation when calculating the offset
	if (!bRotateWithPelvis)
	{
		OutRootTransform.SetRotation(TargetPelvisInRefPose.GetRotation());
	}
	
	// optionally maintain the offset from the ref pose
	if (bMaintainOffsetFromPelvis)
	{
		OutRootTransform = TargetPelvisRelativeToTargetRootRefPose * OutRootTransform;
	}

	// optionally snap the root to the ground plane
	if (RootHeightSource == ERootMotionHeightSource::SnapToGround)
	{
		FVector RootTranslation = OutRootTransform.GetTranslation();
		RootTranslation.Z = 0.f;
		OutRootTransform.SetTranslation(RootTranslation);
	}

	// optionally snap the root to the height from the source
	if (RootHeightSource == ERootMotionHeightSource::CopyHeightFromSource)
	{
		FVector RootTranslation = OutRootTransform.GetTranslation();
		RootTranslation.Z = InSourceGlobalPose[SourceRootIndex].GetTranslation().Z;
		OutRootTransform.SetTranslation(RootTranslation);
	}
}

void URootMotionGeneratorOp::CopyRootMotionFromSourceRoot(
	FTransform& OutRootTransform,
	const UIKRetargetProcessor* Processor,
	const TArray<FTransform>& InSourceGlobalPose) const
{
	// In this case, we are copying root motion from the source root
	// But we also scale it based on the relative height of the source/target Pelvis
		
	// copy root motion from source root bone
	OutRootTransform = InSourceGlobalPose[SourceRootIndex];
	FVector NewRootLocation = OutRootTransform.GetTranslation();

	// scale root motion by same scale factor applied to Pelvis (and modified by settings)
	FVector RootDelta = NewRootLocation - SourceRootInRefPose.GetTranslation();
	RootDelta *= Processor->GetRootRetargeter().GetGlobalScaleVector();
	NewRootLocation = SourceRootInRefPose.GetTranslation() + RootDelta;

	// optionally snap the root to the ground plane
	if (RootHeightSource == ERootMotionHeightSource::SnapToGround)
	{
		NewRootLocation.Z = 0.f;
	}

	// apply the modified translation
	OutRootTransform.SetTranslation(NewRootLocation);
}

#undef LOCTEXT_NAMESPACE
