// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeMessages.h"
#include "BonePose.h"
#include "Containers/RingBuffer.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "UObject/ObjectKey.h"

struct FAnimInstanceProxy;
class USkeleton;
class UWorld;

namespace UE::PoseSearch
{

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
extern POSESEARCH_API TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDrawPose;
extern POSESEARCH_API TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDrawTrajectory;
#endif

struct FSearchResult;
typedef uint16 FComponentSpaceTransformIndex;
typedef TPair<FBoneIndexType, FComponentSpaceTransformIndex> FBoneToTransformPair;
typedef TMap<FBoneIndexType, FComponentSpaceTransformIndex> FBoneToTransformMap;

struct FPoseHistoryEntry
{
	// collected bones transforms in component space
	TArray<FQuat4f> ComponentSpaceRotations;
	TArray<FVector> ComponentSpacePositions;
	TArray<FVector3f> ComponentSpaceScales;
	float AccumulatedSeconds = 0.f;

	void Update(float Time, FCSPose<FCompactPose>& ComponentSpacePose, const FBoneToTransformMap& BoneToTransformMap, bool bStoreScales);

	void SetNum(int32 Num, bool bStoreScales);
	int32 Num() const;

	void SetComponentSpaceTransform(int32 Index, const FTransform& Transform);
	FTransform GetComponentSpaceTransform(int32 Index) const;
};

typedef TRingBuffer<FPoseHistoryEntry> FPoseHistoryEntries;
typedef TArray<FPoseHistoryEntry> FPoseHistoryFutureEntries;

struct POSESEARCH_API IPoseHistory
{
public:
	virtual ~IPoseHistory() {}
	
	// returns the BoneIndexType transform relative to ReferenceBoneIndexType: 
	// if ReferenceBoneIndexType is 0 (RootBoneIndexType), OutBoneTransform is in root bone space
	// if ReferenceBoneIndexType is FBoneIndexType(-1) (ComponentSpaceIndexType), OutBoneTransform is in component space
	// if ReferenceBoneIndexType is FBoneIndexType(-2) (WorldSpaceIndexType), OutBoneTransform is in world space
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = true) const = 0;
	virtual const FPoseSearchQueryTrajectory& GetTrajectory() const = 0;
	
	// @todo: deprecate this API. TrajectorySpeedMultiplier should be a global query scaling value passed as input parameter of FSearchContext during config BuildQuery
	virtual float GetTrajectorySpeedMultiplier() const = 0;
	virtual bool IsEmpty() const = 0;

	virtual const FBoneToTransformMap& GetBoneToTransformMap() const = 0;
	virtual const FPoseHistoryEntries& GetEntries() const = 0;
	virtual const USkeleton* GetLastUpdateSkeleton() const = 0;

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const = 0;
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, float Time, float PointSize = 6.f, bool bExtrapolate = true) const;
#endif
};

struct FPoseHistory : public IPoseHistory
{
	void CacheBones_AnyThread(int32 InNumPoses, float InSamplingInterval, const TArray<FBoneIndexType>& RequiredBones);

	// if bGenerateTrajectory is true, this method will generate trajectory
	// NoTe: InTrajectory.Samples[i].AccumulatedSeconds == 0 is the sample of the previous frame of simulation (since MM works by matching the previous character pose)
	void Update_AnyThread(float DeltaTime, const FPoseSearchQueryTrajectory& InTrajectory, float InTrajectorySpeedMultiplier, bool bGenerateTrajectory, const FAnimInstanceProxy& AnimInstanceProxy, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, bool bNeedsReset);

	void EvaluateComponentSpace_AnyThread(float DeltaTime, FCSPose<FCompactPose>& ComponentSpacePose, bool bGenerateTrajectory, bool bStoreScales);

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const override;
#endif

	// IPoseHistory interface
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = true) const override;
	virtual const FPoseSearchQueryTrajectory& GetTrajectory() const override { return Trajectory; }
	virtual float GetTrajectorySpeedMultiplier() const override { return TrajectorySpeedMultiplier; }
	virtual bool IsEmpty() const override { return Entries.IsEmpty(); }
	virtual const FBoneToTransformMap& GetBoneToTransformMap() const override { return BoneToTransformMap; }
	virtual const FPoseHistoryEntries& GetEntries() const override { return Entries; }
	virtual const USkeleton* GetLastUpdateSkeleton() const override { return LastUpdateSkeleton.Get(); }
	// End of IPoseHistory interface

	static FBoneIndexType GetRemappedBoneIndexType(FBoneIndexType BoneIndexType, const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton);
	static FComponentSpaceTransformIndex GetRemappedComponentSpaceTransformIndex(const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton, const FBoneToTransformMap& BoneToTransformMap, FBoneIndexType BoneIndexType, bool& bSuccess);
	static bool LerpEntries(float Time, bool bExtrapolate, const FPoseHistoryEntry& PrevEntry, const FPoseHistoryEntry& NextEntry, const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton, const FBoneToTransformMap& BoneToTransformMap, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, FTransform& OutBoneTransform);

private:
	// skeleton from the last Update, to keep tracking skeleton changes, and support compatible skeletons
	TWeakObjectPtr<const USkeleton> LastUpdateSkeleton;

	// map of FBoneIndexType(s) to collect. If Empty all the bones get collected
	FBoneToTransformMap BoneToTransformMap;

	// ring buffer of collected bones
	FPoseHistoryEntries Entries;
	
	// caching MaxNumPoses, since Entries.Max() is a padded number
	int32 MaxNumPoses = 0;

	float SamplingInterval = 0.f;

	FPoseSearchQueryTrajectory Trajectory;
	FPoseSearchTrajectoryData::FState TrajectoryDataState;

	// @todo: deprecate this member and expose it via blue print logic or as global query scaling multiplier
	float TrajectorySpeedMultiplier = 1.f;
};

struct FExtendedPoseHistory : public IPoseHistory
{
	void Init(const IPoseHistory* InPoseHistory);

	bool IsInitialized() const;

	// IPoseHistory interface
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = true) const override;
	virtual const FPoseSearchQueryTrajectory& GetTrajectory() const override { check(PoseHistory); return PoseHistory->GetTrajectory(); }
	virtual float GetTrajectorySpeedMultiplier() const override { check(PoseHistory); return PoseHistory->GetTrajectorySpeedMultiplier(); }
	virtual bool IsEmpty() const override { check(PoseHistory); return PoseHistory->IsEmpty() && FutureEntries.IsEmpty(); }
	virtual const FBoneToTransformMap& GetBoneToTransformMap() const override { check(PoseHistory); return PoseHistory->GetBoneToTransformMap(); }
	virtual const FPoseHistoryEntries& GetEntries() const override { check(PoseHistory); return PoseHistory->GetEntries(); }
	virtual const USkeleton* GetLastUpdateSkeleton() const override { check(PoseHistory); return PoseHistory->GetLastUpdateSkeleton(); }
	// End of IPoseHistory interface
	
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const override;
#endif

	void AddFutureRootBone(float Time, const FTransform& FutureRootBoneTransform, bool bStoreScales);
	void AddFuturePose(float Time, FCSPose<FCompactPose>& ComponentSpacePose);

private:
	const IPoseHistory* PoseHistory = nullptr;
	FPoseHistoryFutureEntries FutureEntries;
};

class IPoseHistoryProvider : public UE::Anim::IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE(IPoseHistoryProvider);
public:
	virtual const IPoseHistory& GetPoseHistory() const = 0;
};

struct FHistoricalPoseIndex
{
	bool operator==(const FHistoricalPoseIndex& Index) const
	{
		return PoseIndex == Index.PoseIndex && DatabaseKey == Index.DatabaseKey;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FHistoricalPoseIndex& Index)
	{
		return HashCombineFast(::GetTypeHash(Index.PoseIndex), GetTypeHash(Index.DatabaseKey));
	}

	int32 PoseIndex = INDEX_NONE;
	FObjectKey DatabaseKey;
};

struct FPoseIndicesHistory
{
	void Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime);
	void Reset() { IndexToTime.Reset(); }
	TMap<FHistoricalPoseIndex, float> IndexToTime;
};

} // namespace UE::PoseSearch


