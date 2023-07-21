// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_BlendStack.h"
#include "Algo/MaxElement.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimMontage.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/AnimNode_BlendStackInput.h"
#include "Animation/AnimNode_Inertialization.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_BlendStack)

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimBlendStackEnable(TEXT("a.AnimNode.BlendStack.Enable"), 1, TEXT("Enable / Disable Blend Stack"));
TAutoConsoleVariable<int32> CVarAnimBlendStackPruningEnable(TEXT("a.AnimNode.BlendStack.Pruning.Enable"), 1, TEXT("Enable / Disable Blend Stack Pruning"));
#endif

#define LOCTEXT_NAMESPACE "AnimNode_BlendStack"

/////////////////////////////////////////////////////
// FPoseSearchAnimPlayer
void FPoseSearchAnimPlayer::Initialize(UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, float RootBoneBlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption, FVector BlendParameters, float PlayRate, int32 InPoseLinkIdx)
{
	check(AnimationAsset);

	if (bMirrored && !MirrorDataTable)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchAnimPlayer failed to Initialize for %s. Mirroring will not work becasue MirrorDataTable is missing"), *GetNameSafe(AnimationAsset));
	}

	const FReferenceSkeleton& RefSkeleton = AnimationAsset->GetSkeleton()->GetReferenceSkeleton();
	const bool bApplyDifferentRootBoneBlendTime = RootBoneBlendTime >= 0.f && !FMath::IsNearlyEqual(RootBoneBlendTime, BlendTime);
	const int32 NumSkeletonBones = RefSkeleton.GetNum();
	if (NumSkeletonBones <= 0)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchAnimPlayer failed to Initialize for %s. Skeleton has no bones?!"), *GetNameSafe(AnimationAsset));
	}
	else if (BlendTime > UE_KINDA_SMALL_NUMBER)
	{
		// handling BlendTime > 0 and RootBoneBlendTime >= 0
		if (BlendProfile != nullptr)
		{
			check(BlendProfile->OwningSkeleton && NumSkeletonBones == BlendProfile->OwningSkeleton->GetReferenceSkeleton().GetNum());

			TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);

			BlendProfile->FillSkeletonBoneDurationsArray(TotalBlendInTimePerBone, BlendTime);

			if (bApplyDifferentRootBoneBlendTime)
			{
				TotalBlendInTimePerBone[RootBoneIndexType] *= RootBoneBlendTime / BlendTime;
			}

			BlendTime = *Algo::MaxElement(TotalBlendInTimePerBone);
		}
		else if (bApplyDifferentRootBoneBlendTime)
		{
			TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);
			TotalBlendInTimePerBone[RootBoneIndexType] *= RootBoneBlendTime / BlendTime;
			BlendTime = FMath::Max(BlendTime, RootBoneBlendTime);
		}
	}
	else if (bApplyDifferentRootBoneBlendTime)
	{
		// handling BlendTime ~= 0 and RootBoneBlendTime >= 0
		TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);
		TotalBlendInTimePerBone[RootBoneIndexType] = RootBoneBlendTime;
		BlendTime = FMath::Max(BlendTime, RootBoneBlendTime);
	}

	BlendOption = InBlendOption;

	TotalBlendInTime = BlendTime;
	CurrentBlendInTime = 0.f;

	MirrorNode.SetMirrorDataTable(MirrorDataTable);
	MirrorNode.SetMirror(bMirrored);
	
	if (Cast<UAnimMontage>(AnimationAsset))
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchAnimPlayer unsupported AnimationAsset %s"), *GetNameSafe(AnimationAsset));
	}
	else if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset))
	{
		SequencePlayerNode.SetAccumulatedTime(AccumulatedTime);
		SequencePlayerNode.SetSequence(SequenceBase);
		SequencePlayerNode.SetLoopAnimation(bLoop);
		SequencePlayerNode.SetPlayRate(PlayRate);
	}
	else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset))
	{
		// making sure AccumulatedTime is in normalized space
		check(AccumulatedTime >= 0.f && AccumulatedTime <= 1.f);

		BlendSpacePlayerNode.SetResetPlayTimeWhenBlendSpaceChanges(false /*!bReset*/);
		BlendSpacePlayerNode.SetAccumulatedTime(AccumulatedTime);
		BlendSpacePlayerNode.SetBlendSpace(BlendSpace);
		BlendSpacePlayerNode.SetLoop(bLoop);
		BlendSpacePlayerNode.SetPlayRate(PlayRate);
		BlendSpacePlayerNode.SetPosition(BlendParameters);
	}
	else
	{
		checkNoEntry();
	}

	UpdateSourceLinkNode();
	PoseLinkIndex = InPoseLinkIdx;
}

void FPoseSearchAnimPlayer::UpdatePlayRate(float PlayRate)
{
	if (SequencePlayerNode.GetSequence())
	{
		SequencePlayerNode.SetPlayRate(PlayRate);
	}
	else if (BlendSpacePlayerNode.GetBlendSpace())
	{
		BlendSpacePlayerNode.SetPlayRate(PlayRate);
	}
}

void FPoseSearchAnimPlayer::StorePoseContext(const FPoseContext& PoseContext)
{
	SequencePlayerNode.SetSequence(nullptr);
	BlendSpacePlayerNode.SetBlendSpace(nullptr);
	MirrorNode.SetSourceLinkNode(nullptr);

	if (PoseContext.Pose.IsValid())
	{
		StoredPose.CopyBonesFrom(PoseContext.Pose);
	}

	StoredCurve.CopyFrom(PoseContext.Curve);
	StoredAttributes.CopyFrom(PoseContext.CustomAttributes);
}

void FPoseSearchAnimPlayer::RestorePoseContext(FPoseContext& PoseContext) const
{
	check(!SequencePlayerNode.GetSequence() && !BlendSpacePlayerNode.GetBlendSpace());

	if (StoredPose.IsValid() && PoseContext.Pose.GetNumBones() == StoredPose.GetNumBones())
	{
		PoseContext.Pose.CopyBonesFrom(StoredPose);
	}
	else
	{
		PoseContext.Pose.ResetToRefPose();
	}
	
	PoseContext.Curve.CopyFrom(StoredCurve);
	PoseContext.CustomAttributes.CopyFrom(StoredAttributes);
}


// @todo: maybe implement copy/move constructors and assignment operator do so (or use a list instead of an array)
// since we're making copies and moving this object in memory, we're using this method to set the MirrorNode SourceLinkNode when necessary
void FPoseSearchAnimPlayer::UpdateSourceLinkNode()
{
	if (SequencePlayerNode.GetSequence())
	{
		MirrorNode.SetSourceLinkNode(&SequencePlayerNode);
	}
	else if (BlendSpacePlayerNode.GetBlendSpace())
	{
		MirrorNode.SetSourceLinkNode(&BlendSpacePlayerNode);
	}
	else
	{
		MirrorNode.SetSourceLinkNode(nullptr);
	}
}

void FPoseSearchAnimPlayer::Evaluate_AnyThread(FPoseContext& Output)
{
	if (SequencePlayerNode.GetSequence() || BlendSpacePlayerNode.GetBlendSpace())
	{
		UpdateSourceLinkNode();
		MirrorNode.Evaluate_AnyThread(Output);
	}
	else
	{
		RestorePoseContext(Output);
	}
}

void FPoseSearchAnimPlayer::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	UpdateSourceLinkNode();
	MirrorNode.Update_AnyThread(Context);
}

float FPoseSearchAnimPlayer::GetAccumulatedTime() const
{
	if (SequencePlayerNode.GetSequence())
	{
		return SequencePlayerNode.GetAccumulatedTime();
	}
	
	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		// making sure BlendSpacePlayerNode.GetAccumulatedTime() is in normalized space
		check(BlendSpacePlayerNode.GetAccumulatedTime() >= 0.f && BlendSpacePlayerNode.GetAccumulatedTime() <= 1.f);
		return BlendSpacePlayerNode.GetAccumulatedTime();
	}

	return 0.f;
}

FVector FPoseSearchAnimPlayer::GetBlendParameters() const
{
	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.GetPosition();
	}

	return FVector::ZeroVector;
}

FString FPoseSearchAnimPlayer::GetAnimationName() const
{
	if (SequencePlayerNode.GetSequence())
	{
		check(SequencePlayerNode.GetSequence());
		return SequencePlayerNode.GetSequence()->GetName();
	}

	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		check(BlendSpacePlayerNode.GetBlendSpace());
		return BlendSpacePlayerNode.GetBlendSpace()->GetName();
	}

	return FString("StoredPose");
}

const UAnimationAsset* FPoseSearchAnimPlayer::GetAnimationAsset() const
{
	if (SequencePlayerNode.GetSequence())
	{
		return SequencePlayerNode.GetSequence();
	}

	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.GetBlendSpace();
	}

	return nullptr;
}

float FPoseSearchAnimPlayer::GetBlendInPercentage() const
{
	if (FMath::IsNearlyZero(TotalBlendInTime))
	{
		return 1.f;
	}

	return FMath::Clamp(CurrentBlendInTime / TotalBlendInTime, 0.f, 1.f);
}

bool FPoseSearchAnimPlayer::GetBlendInWeights(TArray<float>& Weights) const
{
	const int32 NumBones = TotalBlendInTimePerBone.Num();
	if (NumBones > 0)
	{
		Weights.SetNumUninitialized(NumBones);
		for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
		{
			const float TotalBlendInTimeBoneIdx = TotalBlendInTimePerBone[BoneIdx];
			if (FMath::IsNearlyZero(TotalBlendInTimeBoneIdx))
			{
				Weights[BoneIdx] = 1.f;
			}
			else
			{
				const float UnclampedLinearWeight = CurrentBlendInTime / TotalBlendInTimeBoneIdx;
				Weights[BoneIdx] = FAlphaBlend::AlphaToBlendOption(UnclampedLinearWeight, BlendOption);
			}
		}
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////
// FAnimNode_BlendStack_Standalone
void FAnimNode_BlendStack_Standalone::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BlendStack_Evaluate_AnyThread);

	Super::Evaluate_AnyThread(Output);

	const int32 BlendStackSize = AnimPlayers.Num();
	if (BlendStackSize <= 0)
	{
		Output.ResetToRefPose();
	}
	else if (BlendStackSize == 1)
	{
		EvaluateSample(Output, 0);
	}
	else if (MaxActiveBlends <= 0 
#if ENABLE_ANIM_DEBUG
		|| !CVarAnimBlendStackEnable.GetValueOnAnyThread()
#endif // ENABLE_ANIM_DEBUG
		)
	{
		// Disable blend stack if requested (for testing / debugging) by removing all the AnimPlayers except the first
		while (AnimPlayers.Num() > 1)
		{
			AnimPlayers.PopLast();
		}
		EvaluateSample(Output, 0);
	}
	else
	{
		// evaluating the last AnimPlayer into Output...
		EvaluateSample(Output, BlendStackSize - 1);

		FPoseContext EvaluationPoseContext(Output);
		FPoseContext BlendedPoseContext(Output); // @todo: this should not be necessary (but FBaseBlendedCurve::InitFrom complains about "ensure(&InCurveToInitFrom != this)"): optimize it away!
		FAnimationPoseData BlendedAnimationPoseData(BlendedPoseContext);

		const USkeleton* SkeletonAsset = Output.AnimInstanceProxy->GetRequiredBones().GetSkeletonAsset();
		check(SkeletonAsset);

		const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
		const int32 NumSkeletonBones = RefSkeleton.GetNum();
		TArray<float> Weights;

		auto EvaluateAndBlendPlayerByIndex = [this, &EvaluationPoseContext, &BlendedAnimationPoseData, &Output, &Weights, &BlendedPoseContext](int32 PlayerIndex)
		{
			// Evaluate into EvaluationPoseContext and then blend it with the Output (initialized with the last AnimPlayer evaluation)
			EvaluateSample(EvaluationPoseContext, PlayerIndex);
			if (AnimPlayers[PlayerIndex].GetBlendInWeights(Weights))
			{
				// @todo: have BlendTwoPosesTogetherPerBone using a TArrayView for the Weights to avoid allocations
				FAnimationRuntime::BlendTwoPosesTogetherPerBone(FAnimationPoseData(Output), FAnimationPoseData(EvaluationPoseContext), Weights, BlendedAnimationPoseData);
			}
			else
			{
				const float Weight = 1.f - FAlphaBlend::AlphaToBlendOption(AnimPlayers[PlayerIndex].GetBlendInPercentage(), AnimPlayers[PlayerIndex].GetBlendOption());
				FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(Output), FAnimationPoseData(EvaluationPoseContext), Weight, BlendedAnimationPoseData);
			}
			Output = BlendedPoseContext; // @todo: this should not be necessary either: optimize it away!
		};

#if ENABLE_ANIM_DEBUG
			const bool bEnablePruning = CVarAnimBlendStackPruningEnable.GetValueOnAnyThread() > 0;
#else
			const bool bEnablePruning = true;
#endif // ENABLE_ANIM_DEBUG

		// Evaluate our players from the second last to the first.
		int32 PlayerIndex = BlendStackSize - 2;
		// Start evaluating with our least significant players.
		for (; PlayerIndex >= MaxActiveBlends; --PlayerIndex)
		{
			EvaluateAndBlendPlayerByIndex(PlayerIndex);

			if (bEnablePruning)
			{
				// too many AnimPlayers! we don't have enough available blends to hold them all, so we accumulate the blended poses into Output / BlendedPoseContext.
				AnimPlayers.PopLast();
			}
		}

		// Even if we're not pruning, we must use the stored pose if we have a limited number of graphs to execute.
		const bool bNeedsStoredPose = (bEnablePruning || !SampleGraphPoseLinks.IsEmpty()) && (PlayerIndex == (MaxActiveBlends - 1));
		if (bNeedsStoredPose)
		{
			check(AnimPlayers.Num() == MaxActiveBlends + 1);

			// We store Output / BlendedPoseContext into the last AnimPlayer, that will hold a static pose, no longer an animation playing.
			AnimPlayers.Last().StorePoseContext(Output);

			if (!SampleGraphPoseLinks.IsEmpty())
			{
				const int32 PoseLinkIdx = AnimPlayers[MaxActiveBlends].GetPoseLinkIndex();
				FBlendStack_SampleGraphPoseLink& PoseLink = SampleGraphPoseLinks[PoseLinkIdx];
				// No players should have evaluated a graph before this point.
				// Evaluate the graph on the blended result.
				PoseLink.EvaluatePlayer(Output, AnimPlayers[MaxActiveBlends]);
			}
		}

		// Continue with our most significant players.
		for (; PlayerIndex >= 0; --PlayerIndex)
		{
			EvaluateAndBlendPlayerByIndex(PlayerIndex);
		}

		const int32 ActiveBlends = AnimPlayers.Num() - 1;
		if (ActiveBlends > MaxActiveBlends)
		{
			UE_LOG(LogPoseSearch, Display, TEXT("FAnimNode_BlendStack_Standalone NumBlends/MaxNumBlends %d / %d"), ActiveBlends, MaxActiveBlends);
		}
	}
}

void FAnimNode_BlendStack_Standalone::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	Reset();

	if (SampleGraphPoseLinks.IsEmpty() == false)
	{
		IAnimClassInterface* AnimBlueprintClass = Context.GetAnimClass();
		check(AnimBlueprintClass);

		// Patch our pose links
		for (FBlendStack_SampleGraphPoseLink& GraphPoseLink : SampleGraphPoseLinks)
		{
			if (GraphPoseLink.RootNodeIndex != INDEX_NONE)
			{
				GraphPoseLink.Root.LinkID = AnimBlueprintClass->GetAnimNodeProperties().Num() - 1 - GraphPoseLink.RootNodeIndex;
			}

			GraphPoseLink.CacheBoneCounter.Reset();
		}
	}
}


void FAnimNode_BlendStack_Standalone::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BlendStack_UpdateAssetPlayer);

	Super::UpdateAssetPlayer(Context);

	// AnimPlayers[0] is the most newly inserted AnimPlayer, AnimPlayers[AnimPlayers.Num()-1] is the oldest, so to calculate the weights
	// we ask AnimPlayers[0] its BlendInPercentage and then distribute the left over (CurrentWeightMultiplier) to the rest of the AnimPlayers
	// AnimPlayers[AnimPlayerIndex].GetBlendWeight() will now store the weighted contribution of AnimPlayers[AnimPlayerIndex] to be able to calculate root motion from animation
	float CurrentWeightMultiplier = 1.f;
	const int32 BlendStackSize = AnimPlayers.Num();
	int32 AnimPlayerIndex = 0;
	for (; AnimPlayerIndex < BlendStackSize; ++AnimPlayerIndex)
	{
		FPoseSearchAnimPlayer& AnimPlayer = AnimPlayers[AnimPlayerIndex];
		const bool bIsLastAnimPlayers = AnimPlayerIndex == BlendStackSize - 1;
		const float BlendInPercentage = bIsLastAnimPlayers ? 1.f : AnimPlayer.GetBlendInPercentage();
		const float AnimPlayerBlendWeight = CurrentWeightMultiplier * BlendInPercentage;

		// don't break for AnimPlayerIndex == 0 since FAnimNode_BlendStack_Standalone::BlendTo initialize the AnimPlayer with a weight of zero
		if (AnimPlayerIndex > 0 && AnimPlayerBlendWeight < UE_KINDA_SMALL_NUMBER)
		{
			break;
		}

		FAnimationUpdateContext AnimPlayerContext = Context.FractionalWeightAndRootMotion(AnimPlayerBlendWeight, AnimPlayerBlendWeight);
		UpdateSample((AnimPlayerIndex == 0) ? AnimPlayerContext : AnimPlayerContext.AsInactive(), AnimPlayerIndex);
		CurrentWeightMultiplier *= (1.f - BlendInPercentage);
	}

	// AnimPlayers[AnimPlayerIndex] is the first FPoseSearchAnimPlayer with a weight contribution of zero, so we can discard it and all the successive AnimPlayers as well
	const int32 WantedAnimPlayersNum = FMath::Max(1, AnimPlayerIndex); // we save at least one FPoseSearchAnimPlayer
	while (AnimPlayers.Num() > WantedAnimPlayersNum)
	{
		AnimPlayers.PopLast();
	}
}

bool FAnimNode_BlendStack_Standalone::IsSampleGraphAvailableForPlayer(const int32 PlayerIndex)
{
	// If we have any sample graphs, our player has been assigned a pose link index.
	// If we are within X most relelvant players, then the graph is available.
	return !SampleGraphPoseLinks.IsEmpty() && (PlayerIndex < MaxActiveBlends);
}

void FAnimNode_BlendStack_Standalone::EvaluateSample(FPoseContext& Output, const int32 PlayerIndex)
{
	FPoseSearchAnimPlayer& SamplePlayer = AnimPlayers[PlayerIndex];
	// If we have any sample graphs, our player has been assigned a pose link index.
	// If we are within X most relelvant players, then the graph is available.
	// If PlayerIndex == MaxActiveBlends, don't evaluate that graph. It's reserved for the stored pose.
	// MaxActiveBlends == 0, means we're using inertialization. Run the the graph.
	const bool bIsSampleGraphAvailable = !SampleGraphPoseLinks.IsEmpty() && 
										((PlayerIndex < MaxActiveBlends) || (MaxActiveBlends == 0));
	if (!bIsSampleGraphAvailable)
	{
		// If we have no sample graph, evaluate the player directly.
		SamplePlayer.Evaluate_AnyThread(Output);
		return;
	}

	FBlendStack_SampleGraphPoseLink& PoseLink = SampleGraphPoseLinks[SamplePlayer.GetPoseLinkIndex()];
	PoseLink.EvaluatePlayer(Output, SamplePlayer);
}

void FBlendStack_SampleGraphPoseLink::EvaluatePlayer(FPoseContext& Output, FPoseSearchAnimPlayer& SamplePlayer)
{
	SetInputPosePlayer(SamplePlayer);

	// Make sure CacheBones has been called before evaluating.
	ConditionalCacheBones(Output);
	// The anim player may or may not have its Evaluate_AnyThread called through the graph update. 
	Root.Evaluate(Output);
}

void FBlendStack_SampleGraphPoseLink::ConditionalCacheBones(const FAnimationBaseContext& Context)
{
	// Only call CacheBones when needed.
	if (!CacheBoneCounter.IsSynchronized_Counter(Context.AnimInstanceProxy->GetCachedBonesCounter()))
	{
		// Keep track of samples that have had CacheBones called on.
		CacheBoneCounter.SynchronizeWith(Context.AnimInstanceProxy->GetCachedBonesCounter());

		FAnimationCacheBonesContext CacheBoneContext(Context.AnimInstanceProxy);
		Root.CacheBones(CacheBoneContext);
	}
}

void FAnimNode_BlendStack_Standalone::UpdateSample(const FAnimationUpdateContext& Context, const int32 PlayerIndex)
{
	FPoseSearchAnimPlayer& SamplePlayer = AnimPlayers[PlayerIndex];

	// If we have any sample graphs, our player has been assigned a pose link index.
	// If we are within X most relelvant players, then the graph is available.
	// @todo: If PlayerIndex == MaxActiveBlends, this will likely become a stored pose. What do we update that graph with? 
	// For now, just use the same player.
	const bool bHasSampleGraph = !SampleGraphPoseLinks.IsEmpty() && (PlayerIndex <= MaxActiveBlends);
	if (bHasSampleGraph)
	{
		FBlendStack_SampleGraphPoseLink& PoseLink = SampleGraphPoseLinks[SamplePlayer.GetPoseLinkIndex()];
		PoseLink.SetInputPosePlayer(SamplePlayer);
		// The anim player may or may not have its Update_AnyThread called through the graph update. 
		PoseLink.Root.Update(Context);
	}
	else
	{
		// If we have no sample graph, update the player directly.
		SamplePlayer.Update_AnyThread(Context);
	}

	// Advance the blend-in time regardless of whether or not the player was updated.
	SamplePlayer.AdvanceBlendInTime(Context.GetDeltaTime());
}

void FAnimNode_BlendStack_Standalone::InitializeSample(const FAnimationInitializeContext& Context, FPoseSearchAnimPlayer& SamplePlayer)
{
	if (SamplePlayer.GetPoseLinkIndex() != INDEX_NONE)
	{
		FBlendStack_SampleGraphPoseLink& PoseLink = SampleGraphPoseLinks[SamplePlayer.GetPoseLinkIndex()];
		PoseLink.Root.Initialize(Context);
	}
}

float FAnimNode_BlendStack_Standalone::GetAccumulatedTime() const
{
	return AnimPlayers.IsEmpty() ? 0.f : AnimPlayers.First().GetAccumulatedTime();
}

static void RequestInertialBlend(const FAnimationUpdateContext& Context, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption BlendOption)
{
	if (BlendTime > 0.0f)
	{
		UE::Anim::IInertializationRequester* InertializationRequester = Context.GetMessage<UE::Anim::IInertializationRequester>();
		if (InertializationRequester)
		{
			FInertializationRequest Request;
			Request.Duration = BlendTime;
			Request.BlendProfile = BlendProfile;
			Request.bUseBlendMode = true;
			Request.BlendMode = BlendOption;
#if ANIM_TRACE_ENABLED
			Request.Description = LOCTEXT("InertializationRequestDescription", "Blend Stack");
			Request.NodeId = Context.GetCurrentNodeId();
			Request.AnimInstance = Context.AnimInstanceProxy->GetAnimInstanceObject();
#endif

			InertializationRequester->RequestInertialization(Request);
		}
		else
		{
			FAnimNode_Inertialization::LogRequestError(Context, Context.GetCurrentNodeId());
		}
	}
}

void FAnimNode_BlendStack_Standalone::BlendTo(const FAnimationUpdateContext& Context, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, float RootBoneBlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption BlendOption, bool bUseInertialBlend, FVector BlendParameters, float PlayRate)
{
	if (bUseInertialBlend)
	{
		RequestInertialBlend(Context, BlendTime, BlendProfile, BlendOption);
		BlendTime = 0.0f;
	}

	AnimPlayers.PushFirst(FPoseSearchAnimPlayer());
	FPoseSearchAnimPlayer& AnimPlayer = AnimPlayers.First();
	AnimPlayer.Initialize(AnimationAsset, AccumulatedTime, bLoop, bMirrored, MirrorDataTable, BlendTime, RootBoneBlendTime, BlendProfile, BlendOption, BlendParameters, PlayRate, GetNextPoseLinkIndex());

	FAnimationInitializeContext InitContext(Context.AnimInstanceProxy, Context.SharedContext);
	InitializeSample(InitContext, AnimPlayer);
}

void FAnimNode_BlendStack_Standalone::Reset()
{
	AnimPlayers.Reset();
}

int32 FAnimNode_BlendStack_Standalone::GetNextPoseLinkIndex()
{
	if (SampleGraphPoseLinks.IsEmpty())
	{
		return INDEX_NONE;
	}

	const int32 NumPoseLinks = SampleGraphPoseLinks.Num();
	CurrentSamplePoseLink = ++CurrentSamplePoseLink;
	if (CurrentSamplePoseLink == NumPoseLinks) { CurrentSamplePoseLink = 0; }

	return CurrentSamplePoseLink;
}

void FAnimNode_BlendStack_Standalone::UpdatePlayRate(float PlayRate)
{
	if (!AnimPlayers.IsEmpty())
	{
		AnimPlayers.First().UpdatePlayRate(PlayRate);
	}
}

void FAnimNode_BlendStack_Standalone::GatherDebugData(FNodeDebugData& DebugData)
{
#if ENABLE_ANIM_DEBUG
	DebugData.AddDebugItem(FString::Printf(TEXT("%s"), *DebugData.GetNodeName(this)));
	for (int32 i = 0; i < AnimPlayers.Num(); ++i)
	{
		const FPoseSearchAnimPlayer& AnimPlayer = AnimPlayers[i];
		DebugData.AddDebugItem(FString::Printf(TEXT("%d) t:%.2f/%.2f m:%d %s"),
				i, AnimPlayer.GetCurrentBlendInTime(), AnimPlayer.GetTotalBlendInTime(),
				AnimPlayer.GetMirror() ? 1 : 0, *AnimPlayer.GetAnimationName()));
	}
#endif // ENABLE_ANIM_DEBUG

	// propagating GatherDebugData to the AnimPlayers
	for (FPoseSearchAnimPlayer& AnimPlayer : AnimPlayers)
	{
		AnimPlayer.GetMirrorNode().GatherDebugData(DebugData);
	}
}

/////////////////////////////////////////////////////
// FAnimNode_BlendStack

void FAnimNode_BlendStack::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	const bool bNeedsReset =
		bResetOnBecomingRelevant &&
		UpdateCounter.HasEverBeenUpdated() &&
		!UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter());

	if (bNeedsReset)
	{
		Reset();
	}

	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	GetEvaluateGraphExposedInputs().Execute(Context);

	if (AnimationAsset)
	{
		bool bExecuteBlendTo = false;
		if (AnimPlayers.IsEmpty())
		{
			bExecuteBlendTo = true;
		}
		else
		{
			const FPoseSearchAnimPlayer& MainAnimPlayer = AnimPlayers.First();
			const UAnimationAsset* PlayingAnimationAsset = MainAnimPlayer.GetAnimationAsset();
			check(PlayingAnimationAsset);

			if (AnimationAsset != PlayingAnimationAsset)
			{
				bExecuteBlendTo = true;
			}
			else if (bMirrored != MainAnimPlayer.GetMirror())
			{
				bExecuteBlendTo = true;
			}
			else if (BlendParameters != MainAnimPlayer.GetBlendParameters())
			{
				bExecuteBlendTo = true;
			}
			else if (MaxAnimationDeltaTime >= 0.f && FMath::Abs(AnimationTime - MainAnimPlayer.GetAccumulatedTime()) > MaxAnimationDeltaTime)
			{
				bExecuteBlendTo = true;
			}
		}

		if (bExecuteBlendTo)
		{
			BlendTo(Context, AnimationAsset, AnimationTime, bLoop, bMirrored, MirrorDataTable.Get(), BlendTime, RootBoneBlendTime, BlendProfile, BlendOption, bUseInertialBlend, BlendParameters, WantedPlayRate);
		}
	}
	
	UpdatePlayRate(WantedPlayRate);

	Super::UpdateAssetPlayer(Context);
}

void FBlendStack_SampleGraphPoseLink::SetInputPosePlayer(FPoseSearchAnimPlayer& InPlayer)
{
	// Because our anim players may get reallocated, or change indices due to push/pops,
	// we must call this before every operation that might end up needing the anim player through the graph's input nodes.
	Player = &InPlayer;
}

#undef LOCTEXT_NAMESPACE