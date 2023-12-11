// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchHistory.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "BonePose.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"

IMPLEMENT_ANIMGRAPH_MESSAGE(UE::PoseSearch::IPoseHistoryProvider);

namespace UE::PoseSearch
{

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDrawPose(TEXT("a.AnimNode.PoseHistory.DebugDrawPose"), false, TEXT("Enable / Disable Pose History Pose DebugDraw"));
TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDrawTrajectory(TEXT("a.AnimNode.PoseHistory.DebugDrawTrajectory"), false, TEXT("Enable / Disable Pose History Trajectory DebugDraw"));
#endif

/**
* Algo::LowerBound adapted to TIndexedContainerIterator for use with indexable but not necessarily contiguous containers. Used here with TRingBuffer.
*
* Performs binary search, resulting in position of the first element >= Value using predicate
*
* @param First TIndexedContainerIterator beginning of range to search through, must be already sorted by SortPredicate
* @param Last TIndexedContainerIterator end of range
* @param Value Value to look for
* @param SortPredicate Predicate for sort comparison, defaults to <
*
* @returns Position of the first element >= Value, may be position after last element in range
*/
template <typename IteratorType, typename ValueType, typename ProjectionType, typename SortPredicateType>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate) -> decltype(First.GetIndex())
{
	using SizeType = decltype(First.GetIndex());

	check(First.GetIndex() <= Last.GetIndex());

	// Current start of sequence to check
	SizeType Start = First.GetIndex();

	// Size of sequence to check
	SizeType Size = Last.GetIndex() - Start;

	// With this method, if Size is even it will do one more comparison than necessary, but because Size can be predicted by the CPU it is faster in practice
	while (Size > 0)
	{
		const SizeType LeftoverSize = Size % 2;
		Size = Size / 2;

		const SizeType CheckIndex = Start + Size;
		const SizeType StartIfLess = CheckIndex + LeftoverSize;

		auto&& CheckValue = Invoke(Projection, *(First + CheckIndex));
		Start = SortPredicate(CheckValue, Value) ? StartIfLess : Start;
	}
	return Start;
}

template <typename IteratorType, typename ValueType, typename SortPredicateType = TLess<>()>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value, SortPredicateType SortPredicate) -> decltype(First.GetIndex())
{
	return LowerBound(First, Last, Value, FIdentityFunctor(), SortPredicate);
}

//////////////////////////////////////////////////////////////////////////
// FPoseHistoryEntry
void FPoseHistoryEntry::Update(float Time, FCSPose<FCompactPose>& ComponentSpacePose, const FBoneToTransformMap& BoneToTransformMap, bool bStoreScales)
{
	AccumulatedSeconds = Time;

	const FBoneContainer& BoneContainer = ComponentSpacePose.GetPose().GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	check(SkeletonAsset);
	const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
	const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();

	if (BoneToTransformMap.IsEmpty())
	{
		// no mapping: we add all the transforms
		SetNum(NumSkeletonBones, bStoreScales);
		for (FSkeletonPoseBoneIndex SkeletonBoneIdx(0); SkeletonBoneIdx != NumSkeletonBones; ++SkeletonBoneIdx)
		{
			const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
			SetComponentSpaceTransform(SkeletonBoneIdx.GetInt(), (CompactBoneIdx.IsValid() ? ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIdx) : RefBonePose[SkeletonBoneIdx.GetInt()]));
		}
	}
	else
	{
		SetNum(BoneToTransformMap.Num(), true);
		for (const FBoneToTransformPair& BoneToTransformPair : BoneToTransformMap)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIdx(BoneToTransformPair.Key);
			const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
			SetComponentSpaceTransform(BoneToTransformPair.Value, (CompactBoneIdx.IsValid() ? ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIdx) : RefBonePose[SkeletonBoneIdx.GetInt()]));
		}
	}
}

void FPoseHistoryEntry::SetNum(int32 Num, bool bStoreScales)
{
	ComponentSpaceRotations.SetNum(Num);
	ComponentSpacePositions.SetNum(Num);
	ComponentSpaceScales.SetNum(bStoreScales ? Num : 0);
}

int32 FPoseHistoryEntry::Num() const
{
	return ComponentSpaceRotations.Num();
}

void FPoseHistoryEntry::SetComponentSpaceTransform(int32 Index, const FTransform& Transform)
{
	ComponentSpaceRotations[Index] = FQuat4f(Transform.GetRotation());
	ComponentSpacePositions[Index] = Transform.GetTranslation();
	
	if (!ComponentSpaceScales.IsEmpty())
	{
		ComponentSpaceScales[Index] = FVector3f(Transform.GetScale3D());
	}
}

FTransform FPoseHistoryEntry::GetComponentSpaceTransform(int32 Index) const
{
	const FQuat Quat(ComponentSpaceRotations[Index]);
	const FVector Scale(ComponentSpaceScales.IsEmpty() ? FVector3f::OneVector : ComponentSpaceScales[Index]);
	return FTransform(Quat, ComponentSpacePositions[Index], Scale);
}

//////////////////////////////////////////////////////////////////////////
// IPoseHistory
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void IPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, float Time, float PointSize, bool bExtrapolate) const
{
	const FBoneContainer& BoneContainer = AnimInstanceProxy.GetRequiredBones();
	if (BoneContainer.IsValid())
	{
		const USkeleton* Skeleton = BoneContainer.GetSkeletonAsset();
		FTransform OutBoneTransform;

		const FBoneToTransformMap& BoneToTransformMap = GetBoneToTransformMap();
		if (BoneToTransformMap.IsEmpty())
		{
			for (FSkeletonPoseBoneIndex SkeletonBoneIdx(0); SkeletonBoneIdx != BoneContainer.GetNumBones(); ++SkeletonBoneIdx)
			{
				const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
				if (GetTransformAtTime(Time, OutBoneTransform, Skeleton, CompactBoneIdx.GetInt(), WorldSpaceIndexType, bExtrapolate))
				{
					AnimInstanceProxy.AnimDrawDebugPoint(OutBoneTransform.GetTranslation(), PointSize, Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}
			}
		}
		else
		{
			for (const FBoneToTransformPair& BoneToTransformPair : BoneToTransformMap)
			{
				const FSkeletonPoseBoneIndex SkeletonBoneIdx(BoneToTransformPair.Key);
				const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
				if (GetTransformAtTime(Time, OutBoneTransform, Skeleton, CompactBoneIdx.GetInt(), WorldSpaceIndexType, bExtrapolate))
				{
					AnimInstanceProxy.AnimDrawDebugPoint(OutBoneTransform.GetTranslation(), PointSize, Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FPoseHistory
void FPoseHistory::CacheBones_AnyThread(int32 InNumPoses, float InSamplingInterval, const TArray<FBoneIndexType>& RequiredBones)
{
	check(InNumPoses >= 2 && InSamplingInterval > UE_KINDA_SMALL_NUMBER);

	MaxNumPoses = InNumPoses;
	SamplingInterval = InSamplingInterval;

	BoneToTransformMap.Reset();
	if (!RequiredBones.IsEmpty())
	{
		// making sure we always collect the root bone transform (by construction BoneToTransformMap[0] = 0)
		BoneToTransformMap.Add(RootBoneIndexType) = BoneToTransformMap.Num();

		for (int32 i = 0; i < RequiredBones.Num(); ++i)
		{
			// adding only unique RequiredBones to avoid oversizing Entries::ComponentSpaceTransforms
			if (!BoneToTransformMap.Find(RequiredBones[i]))
			{
				BoneToTransformMap.Add(RequiredBones[i]) = BoneToTransformMap.Num();
			}
		}
	}

	Entries.Reset();
	Entries.Reserve(MaxNumPoses);
}

FBoneIndexType FPoseHistory::GetRemappedBoneIndexType(FBoneIndexType BoneIndexType, const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton)
{
	// remapping BoneIndexType in case the skeleton used to store history (LastUpdateSkeleton) is different from BoneIndexSkeleton
	if (LastUpdateSkeleton != nullptr && LastUpdateSkeleton != BoneIndexSkeleton)
	{
		const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(BoneIndexSkeleton, LastUpdateSkeleton);
		if (SkeletonRemapping.IsValid())
		{
			BoneIndexType = SkeletonRemapping.GetTargetSkeletonBoneIndex(BoneIndexType);
		}
	}

	return BoneIndexType;
}

FComponentSpaceTransformIndex FPoseHistory::GetRemappedComponentSpaceTransformIndex(const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton, const FBoneToTransformMap& BoneToTransformMap, FBoneIndexType BoneIndexType, bool& bSuccess)
{
	check(BoneIndexType != WorldSpaceIndexType);

	FComponentSpaceTransformIndex BoneTransformIndex = FComponentSpaceTransformIndex(BoneIndexType);
	if (BoneIndexType != ComponentSpaceIndexType)
	{
		BoneIndexType = GetRemappedBoneIndexType(BoneIndexType, BoneIndexSkeleton, LastUpdateSkeleton);

		if (!BoneToTransformMap.IsEmpty())
		{
			if (const FComponentSpaceTransformIndex* FoundBoneTransformIndex = BoneToTransformMap.Find(BoneTransformIndex))
			{
				BoneTransformIndex = *FoundBoneTransformIndex;
			}
			else
			{
				BoneTransformIndex = RootBoneIndexType;
				bSuccess = false;
			}
		}
	}
	return BoneTransformIndex;
}

bool FPoseHistory::LerpEntries(float Time, bool bExtrapolate, const FPoseHistoryEntry& PrevEntry, const FPoseHistoryEntry& NextEntry, const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton,
	const FBoneToTransformMap& BoneToTransformMap, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, FTransform& OutBoneTransform)
{
	bool bSuccess = true;

	const float Denominator = NextEntry.AccumulatedSeconds - PrevEntry.AccumulatedSeconds;
	float LerpValue = 0.f;
	if (!FMath::IsNearlyZero(Denominator))
	{
		const float Numerator = Time - PrevEntry.AccumulatedSeconds;
		LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
	}

	const FComponentSpaceTransformIndex BoneTransformIndex = GetRemappedComponentSpaceTransformIndex(BoneIndexSkeleton, LastUpdateSkeleton, BoneToTransformMap, BoneIndexType, bSuccess);
	const FComponentSpaceTransformIndex ReferenceBoneTransformIndex = GetRemappedComponentSpaceTransformIndex(BoneIndexSkeleton, LastUpdateSkeleton, BoneToTransformMap, ReferenceBoneIndexType, bSuccess);

	if (BoneTransformIndex != ComponentSpaceIndexType)
	{
		if (ReferenceBoneTransformIndex == ComponentSpaceIndexType)
		{
			OutBoneTransform.Blend(
				PrevEntry.GetComponentSpaceTransform(BoneTransformIndex),
				NextEntry.GetComponentSpaceTransform(BoneTransformIndex),
				LerpValue);
		}
		else
		{
			OutBoneTransform.Blend(
				PrevEntry.GetComponentSpaceTransform(BoneTransformIndex) * PrevEntry.GetComponentSpaceTransform(ReferenceBoneTransformIndex).Inverse(),
				NextEntry.GetComponentSpaceTransform(BoneTransformIndex) * NextEntry.GetComponentSpaceTransform(ReferenceBoneTransformIndex).Inverse(),
				LerpValue);
		}
	}
	else
	{
		// @todo: implement if required
		OutBoneTransform = FTransform::Identity;
		bSuccess = false;
		unimplemented();
	}

	return bSuccess;
}

bool FPoseHistory::GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, bool bExtrapolate) const
{
	static_assert(RootBoneIndexType == 0 && ComponentSpaceIndexType == FBoneIndexType(-1) && WorldSpaceIndexType == FBoneIndexType(-2)); // some assumptions
	check(BoneIndexType != ComponentSpaceIndexType && BoneIndexType != WorldSpaceIndexType);
	
	bool bSuccess = false;
	
	const bool bApplyComponentToWorld = ReferenceBoneIndexType == WorldSpaceIndexType;
	FTransform ComponentToWorld = FTransform::Identity;
	if (bApplyComponentToWorld)
	{
		ComponentToWorld = Trajectory.GetSampleAtTime(Time, bExtrapolate).GetTransform();
		ReferenceBoneIndexType = ComponentSpaceIndexType;
	}

	const int32 NumEntries = Entries.Num();
	if (NumEntries > 0)
	{
		int32 NextIdx = 0;
		int32 PrevIdx = 0;

		if (NumEntries > 1)
		{
			const int32 LowerBoundIdx = LowerBound(Entries.begin(), Entries.end(), Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
			NextIdx = FMath::Clamp(LowerBoundIdx, 1, NumEntries - 1);
			PrevIdx = NextIdx - 1;
		}
	
		const FPoseHistoryEntry& PrevEntry = Entries[PrevIdx];
		const FPoseHistoryEntry& NextEntry = Entries[NextIdx];

		bSuccess = LerpEntries(Time, bExtrapolate, PrevEntry, NextEntry, BoneIndexSkeleton, GetLastUpdateSkeleton(), BoneToTransformMap, BoneIndexType, ReferenceBoneIndexType, OutBoneTransform);
		if (bApplyComponentToWorld)
		{
			OutBoneTransform *= ComponentToWorld;
		}
	}
	else
	{
		OutBoneTransform = ComponentToWorld;
	}
	
	return bSuccess;
}

void FPoseHistory::Update_AnyThread(float DeltaTime, const FPoseSearchQueryTrajectory& InTrajectory, float InTrajectorySpeedMultiplier, bool bGenerateTrajectory, const FAnimInstanceProxy& AnimInstanceProxy, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, bool bNeedsReset)
{
	if (bNeedsReset)
	{
		Entries.Reset();
	}

	if (bGenerateTrajectory)
	{
		// @todo: Synchronize the FPoseSearchQueryTrajectorySample::AccumulatedSeconds of the generated trajectory with the FPoseHistoryEntry::AccumulatedSeconds of the captured poses
		FPoseSearchTrajectoryData::FDerived TrajectoryDataDerived;
		TrajectoryData.UpdateData(DeltaTime, AnimInstanceProxy, TrajectoryDataDerived, TrajectoryDataState);
		FPoseSearchTrajectoryLibrary::InitTrajectorySamples(Trajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling);
		FPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(Trajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, DeltaTime);
		FPoseSearchTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(Trajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling);

		// @todo: support TrajectorySpeedMultiplier
		TrajectorySpeedMultiplier = 1.f;
	}
	else
	{
		TrajectoryDataState = FPoseSearchTrajectoryData::FState();
	Trajectory = InTrajectory;

		TrajectorySpeedMultiplier = InTrajectorySpeedMultiplier;
	if (!FMath::IsNearlyEqual(TrajectorySpeedMultiplier, 1.f))
	{
		const float TrajectorySpeedMultiplierInv = FMath::IsNearlyZero(TrajectorySpeedMultiplier) ? 1.f : 1.f / TrajectorySpeedMultiplier;
		for (FPoseSearchQueryTrajectorySample& Sample : Trajectory.Samples)
		{
			Sample.AccumulatedSeconds *= TrajectorySpeedMultiplierInv;
		}
	}
}
}

void FPoseHistory::EvaluateComponentSpace_AnyThread(float DeltaTime, FCSPose<FCompactPose>& ComponentSpacePose, bool bGenerateTrajectory, bool bStoreScales)
{
	const USkeleton* Skeleton = ComponentSpacePose.GetPose().GetBoneContainer().GetSkeletonAsset();
	if (LastUpdateSkeleton != Skeleton)
	{
		// @todo: support a different USkeleton per FPoseHistoryEntry if required
		Entries.Reset();
		LastUpdateSkeleton = Skeleton;
	}

	// Age our elapsed times
	for (FPoseHistoryEntry& Entry : Entries)
	{
		Entry.AccumulatedSeconds -= DeltaTime;
	}

	if (Entries.Num() != MaxNumPoses)
	{
		// Consume every pose until the queue is full
		Entries.Emplace();
	}
	else if (Entries[Entries.Num() - 2].AccumulatedSeconds <= -SamplingInterval)
	{
		FPoseHistoryEntry EntryTemp = MoveTemp(Entries.First());
		Entries.PopFront();
		Entries.Emplace(MoveTemp(EntryTemp));
	}

	// Regardless of the retention policy, we always update the most recent Entry
	Entries.Last().Update(0.f, ComponentSpacePose, BoneToTransformMap, bStoreScales);
}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const
{
	if (CVarAnimPoseHistoryDebugDrawTrajectory.GetValueOnAnyThread())
	{
		Trajectory.DebugDrawTrajectory(AnimInstanceProxy);
	}

	if (Color.A > 0 && CVarAnimPoseHistoryDebugDrawPose.GetValueOnAnyThread())
{
		const bool bValidTrajectory = !Trajectory.Samples.IsEmpty();
		TArray<FTransform, TInlineAllocator<128>> PrevGlobalTransforms;

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FPoseHistoryEntry& Entry = Entries[EntryIndex];

			const int32 PrevGlobalTransformsNum = PrevGlobalTransforms.Num();
			const int32 Max = FMath::Max(PrevGlobalTransformsNum, Entry.Num());

			PrevGlobalTransforms.SetNum(Max, false);

			for (int32 i = 0; i < Entry.Num(); ++i)
			{
				const FTransform RootTransform = bValidTrajectory ? Trajectory.GetSampleAtTime(Entry.AccumulatedSeconds).GetTransform() : AnimInstanceProxy.GetComponentTransform();
				const FTransform GlobalTransforms = Entry.GetComponentSpaceTransform(i) * RootTransform;

				if (i < PrevGlobalTransformsNum)
				{
					AnimInstanceProxy.AnimDrawDebugLine(PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FExtendedPoseHistory
void FExtendedPoseHistory::Init(const IPoseHistory* InPoseHistory)
{
	check(InPoseHistory);
	PoseHistory = InPoseHistory;
}

bool FExtendedPoseHistory::IsInitialized() const
{
	return PoseHistory != nullptr;
}

bool FExtendedPoseHistory::GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, bool bExtrapolate) const
{
	check(PoseHistory);
	if (Time > 0.f)
	{
		const int32 Num = FutureEntries.Num();
		if (Num > 0)
		{
			const bool bApplyComponentToWorld = ReferenceBoneIndexType == WorldSpaceIndexType;
			FTransform ComponentToWorld = FTransform::Identity;
			if (bApplyComponentToWorld)
			{
				ComponentToWorld = GetTrajectory().GetSampleAtTime(Time, bExtrapolate).GetTransform();
				ReferenceBoneIndexType = ComponentSpaceIndexType;
			}

			const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
			const int32 NextIdx = FMath::Min(LowerBoundIdx, Num - 1);
			const FPoseHistoryEntries& PastEntries = GetEntries();
			const FPoseHistoryEntry& NextEntry = FutureEntries[NextIdx];
			const FPoseHistoryEntry& PrevEntry = NextIdx > 0 ? FutureEntries[NextIdx - 1] : !PastEntries.IsEmpty() ? PastEntries.First() : NextEntry;
						
			const bool bSuccess = FPoseHistory::LerpEntries(Time, bExtrapolate, PrevEntry, NextEntry, BoneIndexSkeleton, GetLastUpdateSkeleton(), GetBoneToTransformMap(), BoneIndexType, ReferenceBoneIndexType, OutBoneTransform);
			if (bApplyComponentToWorld)
			{
				OutBoneTransform *= ComponentToWorld;
			}
			return bSuccess;
		}
	}
	
	return PoseHistory->GetTransformAtTime(Time, OutBoneTransform, BoneIndexSkeleton, BoneIndexType, ReferenceBoneIndexType, bExtrapolate);
}

void FExtendedPoseHistory::AddFutureRootBone(float Time, const FTransform& FutureRootBoneTransform, bool bStoreScales)
{
	// we don't allow to add "past" or "present" poses to FutureEntries
	check(Time > 0.f);

	const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
	FPoseHistoryEntry& FutureEntry = FutureEntries.InsertDefaulted_GetRef(LowerBoundIdx);
	FutureEntry.SetNum(1, bStoreScales);
	FutureEntry.SetComponentSpaceTransform(RootBoneIndexType, FutureRootBoneTransform);
	FutureEntry.AccumulatedSeconds = Time;
}

void FExtendedPoseHistory::AddFuturePose(float Time, FCSPose<FCompactPose>& ComponentSpacePose)
{
	// we don't allow to add "past" or "present" poses to FutureEntries
	check(Time > 0.f);
	check(PoseHistory);	
	const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
	FutureEntries.InsertDefaulted_GetRef(LowerBoundIdx).Update(Time, ComponentSpacePose, GetBoneToTransformMap(), true);
}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FExtendedPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const
{
	check(PoseHistory);

	if (Color.A > 0 && !FutureEntries.IsEmpty() && CVarAnimPoseHistoryDebugDrawPose.GetValueOnAnyThread())
	{
		const FPoseSearchQueryTrajectory& Trajectory = GetTrajectory();
		const bool bValidTrajectory = !Trajectory.Samples.IsEmpty();
		TArray<FTransform, TInlineAllocator<128>> PrevGlobalTransforms;

		const FPoseHistoryEntries& PastEntries = GetEntries();

		int32 EntriesNum = FutureEntries.Num();
		if (!PastEntries.IsEmpty())
		{
			// connecting the future entries with the past entries
			++EntriesNum;
		}

		for (int32 EntryIndex = 0; EntryIndex < EntriesNum; ++EntryIndex)
		{
			const FPoseHistoryEntry& Entry = (EntryIndex == FutureEntries.Num()) ? PastEntries.Last() : FutureEntries[EntryIndex];

			const int32 PrevGlobalTransformsNum = PrevGlobalTransforms.Num();
			const int32 Max = FMath::Max(PrevGlobalTransformsNum, Entry.Num());

			PrevGlobalTransforms.SetNum(Max, false);

			for (int32 i = 0; i < Entry.Num(); ++i)
			{
				const FTransform RootTransform = bValidTrajectory ? Trajectory.GetSampleAtTime(Entry.AccumulatedSeconds).GetTransform() : AnimInstanceProxy.GetComponentTransform();
				const FTransform GlobalTransforms = Entry.GetComponentSpaceTransform(i) * RootTransform;

				if (i < PrevGlobalTransformsNum)
				{
					AnimInstanceProxy.AnimDrawDebugLine(PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}

		// no need to DebugDraw PoseHistory since it'll be drawn anyways by the history collectors
		//PoseHistory->DebugDraw(AnimInstanceProxy, Color);
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FPoseIndicesHistory
void FPoseIndicesHistory::Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime)
{
	if (MaxTime > 0.f)
	{
		for (auto It = IndexToTime.CreateIterator(); It; ++It)
		{
			It.Value() += DeltaTime;
			if (It.Value() > MaxTime)
			{
				It.RemoveCurrent();
			}
		}

		if (SearchResult.IsValid())
		{
			FHistoricalPoseIndex HistoricalPoseIndex;
			HistoricalPoseIndex.PoseIndex = SearchResult.PoseIdx;
			HistoricalPoseIndex.DatabaseKey = FObjectKey(SearchResult.Database.Get());
			IndexToTime.Add(HistoricalPoseIndex, 0.f);
		}
	}
	else
	{
		IndexToTime.Reset();
	}
}

} // namespace UE::PoseSearch
