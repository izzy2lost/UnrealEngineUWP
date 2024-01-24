// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneControllers/BoneControllerTypes.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_Steering.generated.h"

struct FAnimationInitializeContext;
struct FComponentSpacePoseContext;
struct FNodeDebugData;

// add procedural delta to the root motion attribute 
USTRUCT(BlueprintInternalUseOnly)
struct ANIMATIONWARPINGRUNTIME_API FAnimNode_Steering : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	// The Orientation to steer towards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	FQuat TargetOrientation = FQuat::Identity;
	
	// The number of seconds in the future before we should reach the TargetOrientation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	float TargetTime = 0.2f;

	// If less than this number of degrees of rotation is found over TargetTime seconds in the CurrentAnimAsset, then rotation will be added linearly to reach the TargetOrientation
	// Otherwise the root motion will be scaled to reach the TargetOrientation over TargetTime seconds
	UPROPERTY(EditAnywhere, Category=Evaluation)
	float RootMotionThreshold = 1.0f;
	
	
	
	// Animation Asset for incorporating root motion data. If CurrentAnimAsset is set, and the animation has root motion rotation within the TargetTime, then those rotations will be scaled to reach the TargetOrientation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	TObjectPtr<UAnimationAsset> CurrentAnimAsset;
	
	// Current playback time in seconds of the CurrentAnimAsset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	float CurrentAnimAssetTime = 0.f;

	// FAnimNodeBase interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	// End of FAnimNodeBase interface
	
	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override { return true; }
	// End of FAnimNode_SkeletalControlBase interface

private:

	FTransform RootBoneTransform;
};
