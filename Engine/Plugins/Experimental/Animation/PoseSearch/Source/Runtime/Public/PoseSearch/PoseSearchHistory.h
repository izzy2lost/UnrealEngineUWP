// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeMessages.h"
#include "BonePose.h"
#include "Containers/RingBuffer.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "UObject/ObjectKey.h"

struct FAnimInstanceProxy;
struct FPoseSearchQueryTrajectory;
class USkeleton;
class UWorld;

namespace UE::PoseSearch
{

struct FSearchResult;
typedef uint16 FComponentSpaceTransformIndex;
typedef TPair<FBoneIndexType, FComponentSpaceTransformIndex> FBoneToTransformPair;
typedef TMap<FBoneIndexType, FComponentSpaceTransformIndex> FBoneToTransformMap;

struct POSESEARCH_API IPoseHistory
{
	virtual ~IPoseHistory() {}
	
	// returns the BoneIndexType transform relative to ReferenceBoneIndexType: 
	// if ReferenceBoneIndexType is 0 (RootBoneIndexType), OutBoneTransform is in root bone space
	// if ReferenceBoneIndexType is FBoneIndexType(-1) (ComponentSpaceIndexType), OutBoneTransform is in component space
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = true) const = 0;
	virtual bool IsEmpty() const = 0;
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	// debug draw the pose history. If Trajectory is not provided all the poses will be drawn at the AnimInstanceProxy transform
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, const FPoseSearchQueryTrajectory* Trajectory = nullptr) const = 0;
#endif
};

struct FPoseHistoryEntry
{
	// collected bones transforms in component space
	TArray<FQuat4f> ComponentSpaceRotations;
	TArray<FVector> ComponentSpacePositions;
	TArray<FVector3f> ComponentSpaceScales;
	float Time = 0.f;

	void Update(float InTime, FCSPose<FCompactPose>& ComponentSpacePose, const FBoneToTransformMap& BoneToTransformMap, bool bStoreScales);

	void SetNum(int32 Num, bool bStoreScales);
	int32 Num() const;

	void SetComponentSpaceTransform(int32 Index, const FTransform& Transform);
	FTransform GetComponentSpaceTransform(int32 Index) const;
};

typedef TRingBuffer<FPoseHistoryEntry> FPoseHistoryEntries;
typedef TArray<FPoseHistoryEntry> FPoseHistoryFutureEntries;

struct FPoseHistory : public IPoseHistory
{
	void Init(int32 InNumPoses, float InSamplingInterval, const TArray<FBoneIndexType>& RequiredBones);
	void Update(float SecondsElapsed, FCSPose<FCompactPose>& ComponentSpacePose, bool bStoreScales);

	const FBoneToTransformMap& GetBoneToTransformMap() const { return BoneToTransformMap; }
	const FPoseHistoryEntries& GetEntries() const { return Entries; }
	const USkeleton* GetLastUpdateSkeleton() const { return LastUpdateSkeleton.Get(); }

	// IPoseHistory interface
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = true) const override;
	virtual bool IsEmpty() const override;
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, const FPoseSearchQueryTrajectory* Trajectory = nullptr) const override;
#endif
	// End of IPoseHistory interface

	void ClearHistory();

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
	float SamplingInterval = 0.f;
};

struct FExtendedPoseHistory : public IPoseHistory
{
	void Init(const FPoseHistory* InPoseHistory);

	bool IsInitialized() const;

	// IPoseHistory interface
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = true) const override;
	virtual bool IsEmpty() const override;
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, const FPoseSearchQueryTrajectory* Trajectory = nullptr) const override;	
#endif
	// End of IPoseHistory interface

	void ResetFuturePoses();
	void AddFuturePose(float SecondsInTheFuture, FCSPose<FCompactPose>& ComponentSpacePose, const FTransform& ComponentTransform);

private:
	const FPoseHistory* PoseHistory = nullptr;
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


