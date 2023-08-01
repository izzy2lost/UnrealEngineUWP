// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Mirror.h"
#include "Containers/Deque.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "AnimNode_BlendStack.generated.h"

USTRUCT()
struct FPoseSearchAnimPlayer
{
	GENERATED_BODY()
	
	void Initialize(UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, float RootBoneBlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption, FVector BlendParameters, float PlayRate, int32 InPoseLinkIdx);
	void UpdatePlayRate(float PlayRate);
	void Evaluate_AnyThread(FPoseContext& Output);
	void Update_AnyThread(const FAnimationUpdateContext& Context);
	float GetAccumulatedTime() const;
	
	float GetBlendInPercentage() const;
	bool GetBlendInWeights(TArray<float>& Weights) const;
	EAlphaBlendOption GetBlendOption() const { return BlendOption; }
	void StorePoseContext(const FPoseContext& PoseContext);

	float GetTotalBlendInTime() const { return TotalBlendInTime; }
	float GetCurrentBlendInTime() const { return CurrentBlendInTime; }
	void AdvanceBlendInTime(const float DeltaTime) { CurrentBlendInTime += DeltaTime; }
	bool GetMirror() const { return MirrorNode.GetMirror(); }
	FVector GetBlendParameters() const;
	FString GetAnimationName() const;
	const UAnimationAsset* GetAnimationAsset() const;

	FAnimNode_Mirror_Standalone& GetMirrorNode() { return MirrorNode; }
	int32 GetPoseLinkIndex() const { return PoseLinkIndex; }

public:
	void RestorePoseContext(FPoseContext& PoseContext) const;
	void UpdateSourceLinkNode();

	// Embedded standalone player to play sequence
	FAnimNode_SequencePlayer_Standalone SequencePlayerNode;

	// Embedded standalone player to play blend spaces
	FAnimNode_BlendSpacePlayer_Standalone BlendSpacePlayerNode;

	// Embedded mirror node to handle mirroring if the pose search results in a mirrored sequence
	FAnimNode_Mirror_Standalone MirrorNode;

	// if SequencePlayerNode.GetSequence() and BlendSpacePlayerNode.GetBlendSpace() are nullptr, 
	// instead of using SequencePlayerNode or BlendSpacePlayerNode (wrapped in MirrorNode),
	// the output FPoseContext will be from StoredPose, StoredCurve, StoredAttributes
	FCompactHeapPose StoredPose;
	FBlendedHeapCurve StoredCurve;
	UE::Anim::FHeapAttributeContainer StoredAttributes;
	// We need to store the bone container, in case we have a LOD swap during a blend that uses the stored pose.
	FBoneContainer StoredBoneContainer;

	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;
	int32 PoseLinkIndex = INDEX_NONE;
	TCustomBoneIndexArray<float, FSkeletonPoseBoneIndex> TotalBlendInTimePerBone;

	float TotalBlendInTime = 0.f;
	float CurrentBlendInTime = 0.f;
};

USTRUCT(BlueprintInternalUseOnly)
struct FBlendStack_SampleGraphPoseLink
{
	GENERATED_BODY()

	UPROPERTY()
	int32 RootNodeIndex = INDEX_NONE;

	FPoseLink Root;
	FPoseSearchAnimPlayer* Player = nullptr;
	FGraphTraversalCounter CacheBoneCounter;

	void SetInputPosePlayer(FPoseSearchAnimPlayer& InPlayer);
	void EvaluatePlayer(FPoseContext& Output, FPoseSearchAnimPlayer& SamplePlayer);
	void ConditionalCacheBones(const FAnimationBaseContext& Output);
};

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_BlendStack_Standalone : public FAnimNode_AssetPlayerBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBlendStack_SampleGraphPoseLink> SampleGraphPoseLinks;
	int32 CurrentSamplePoseLink = -1;

	TDeque<FPoseSearchAnimPlayer> AnimPlayers;

	// FAnimNode_Base interface
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	void BlendTo(const FAnimationUpdateContext& Context, UAnimationAsset* AnimationAsset, float AccumulatedTime = 0.f, bool bLoop = false, bool bMirrored = false, UMirrorDataTable* MirrorDataTable = nullptr, float BlendTime = 0.2f, float RootBoneBlendTime = -1.f, const UBlendProfile* BlendProfile = nullptr, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear, bool bUseInertialBlend = false, FVector BlendParameters = FVector::Zero(), float PlayRate = 1.f);
	void UpdatePlayRate(float PlayRate);
	void Reset();

	// FAnimNode_AssetPlayerBase interface
	virtual float GetAccumulatedTime() const override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_AssetPlayerBase interface

	int32 GetNextPoseLinkIndex();

	int32 GetMaxActiveBlends() const { return MaxActiveBlends; }

protected:
	// Number of max active blending animation in the blend stack. If MaxActiveBlends is zero then blend stack is disabled
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	int32 MaxActiveBlends = 4;

private:

	void InitializeSample(const FAnimationInitializeContext& Context, FPoseSearchAnimPlayer& SamplePlayer);
	void EvaluateSample(FPoseContext& Output, const int32 PlayerIndex);
	void UpdateSample(const FAnimationUpdateContext& Context, const int32 PlayerIndex);
	void CacheBonesForSample(const FAnimationCacheBonesContext& Context, const int32 PlayerIndex);
	bool IsSampleGraphAvailableForPlayer(const int32 PlayerIndex);

	friend class UAnimGraphNode_BlendStack_Base;
};


USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_BlendStack : public FAnimNode_BlendStack_Standalone
{
	GENERATED_BODY()

	// requested animation to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	TObjectPtr<UAnimationAsset> AnimationAsset;

	// requested animation time. If negative, the animation will play from the beginning uninterrupted
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float AnimationTime = -1.f;

	// requested AnimationAsset looping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bLoop = true;

	// requested AnimationAsset mirroring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bMirrored = false;

	// requested animation play rate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float WantedPlayRate = 1.f;
	
	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault, ClampMin = "0"))
	float BlendTime = 0.2f;

	// Time in seconds to blend out to the new pose root bone. Negative values would imply RootBoneBlendTime is equal to BlendTime
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float RootBoneBlendTime = -1.f;

	// if AnimationTime and MaxAnimationDeltaTime are positive and the currently playing animation total time differs more than MaxAnimationDeltaTime from AnimationTime
	// (animation desynchronized from the requested time) the blend stack will force a blend into the same animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float MaxAnimationDeltaTime = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault, UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	// requested blend space blend parameters (if AnimationAsset is a blend space)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	FVector BlendParameters = FVector::Zero();

	// if set and bMirrored MirrorDataTable will be used for mirroring the aniamtion
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	TObjectPtr<UMirrorDataTable> MirrorDataTable;

	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bUseInertialBlend = false;

	// Reset the blend stack if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bResetOnBecomingRelevant = true;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

	// FAnimNode_Base interface
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface
};

