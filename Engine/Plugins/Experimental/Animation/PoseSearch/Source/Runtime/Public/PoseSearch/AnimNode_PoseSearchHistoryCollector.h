// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "AnimNode_PoseSearchHistoryCollector.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchHistoryCollector_Base : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	
	// The maximum amount of poses that can be stored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin="2"))
	int32 PoseCount = 64;
	
	// The time horizon for how long a pose will be stored in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin="0.001"))
	float PoseDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FBoneReference> CollectedBones;

	// if true, the pose history will be initialized with a ref pose at the location and orientation of the AnimInstance.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bInitializeWithRefPose = false;

	// Reset the pose history if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bResetOnBecomingRelevant = true;

	UPROPERTY(EditAnywhere, Category = Settings)
	FLinearColor DebugColor = FLinearColor::Red;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

	// FAnimNode_Base interface
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

	const UE::PoseSearch::FPoseHistory& GetPoseHistory() const { return PoseHistory; }

protected:
	UE::PoseSearch::FPoseHistory PoseHistory;
};

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchHistoryCollector : public FAnimNode_PoseSearchHistoryCollector_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink Source;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchComponentSpaceHistoryCollector : public FAnimNode_PoseSearchHistoryCollector_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FComponentSpacePoseLink Source;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};
