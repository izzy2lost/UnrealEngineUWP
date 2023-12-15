// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchResult.h"

struct FPoseSearchQueryTrajectory;
class UPoseSearchDatabase;
class UPoseSearchFeatureChannel_Position;

namespace UE::PoseSearch
{

struct FPoseIndicesHistory;
struct IPoseHistory;

enum class EDebugDrawFlags : uint32
{
	None = 0,

	// Draw using Query colors form the Schema
	DrawQuery = 1 << 1,
};
ENUM_CLASS_FLAGS(EDebugDrawFlags);

enum class EPoseCandidateFlags : uint32
{
	None = 0,

	Valid_Pose = 1 << 0,
	Valid_ContinuingPose = 1 << 1,
	Valid_CurrentPose = 1 << 2,

	AnyValidMask = Valid_Pose | Valid_ContinuingPose | Valid_CurrentPose,

	DiscardedBy_PoseJumpThresholdTime = 1 << 3,
	DiscardedBy_PoseReselectHistory = 1 << 4,
	DiscardedBy_BlockTransition = 1 << 5,
	DiscardedBy_PoseFilter = 1 << 6,
	DiscardedBy_AssetIdxFilter = 1 << 7,
	DiscardedBy_Search = 1 << 8,

	AnyDiscardedMask = DiscardedBy_PoseJumpThresholdTime | DiscardedBy_PoseReselectHistory | DiscardedBy_BlockTransition | DiscardedBy_PoseFilter | DiscardedBy_AssetIdxFilter | DiscardedBy_Search,
};
ENUM_CLASS_FLAGS(EPoseCandidateFlags);

template<typename FTransformType>
struct POSESEARCH_API FCachedTransform
{
	FCachedTransform()
		: SampleTime(0.f)
		, BoneIndexType(RootBoneIndexType)
		, Transform(FTransformType::Identity)
	{
	}

	FCachedTransform(float InSampleTime, FBoneIndexType InBoneIndexType, const FTransformType& InTransform)
		: SampleTime(InSampleTime)
		, BoneIndexType(InBoneIndexType)
		, Transform(InTransform)
	{
	}

	float SampleTime = 0.f;
	
	FBoneIndexType BoneIndexType = RootBoneIndexType;

	// associated transform to BoneIndexType in ComponentSpace (except for the root bone stored in global space)
	FTransformType Transform = FTransformType::Identity;
};

template<typename FTransformType>
struct FCachedTransforms
{
	const FCachedTransform<FTransformType>* Find(float SampleTime, FBoneIndexType BoneIndexType) const
	{
		// @todo: use an hashmap if we end up having too many entries
		return CachedTransforms.FindByPredicate([SampleTime, BoneIndexType](const FCachedTransform<FTransformType>& CachedTransform)
			{
				return CachedTransform.SampleTime == SampleTime && CachedTransform.BoneIndexType == BoneIndexType;
			});
	}

	void Add(float SampleTime, FBoneIndexType BoneIndexType, const FTransformType& Transform)
	{
		CachedTransforms.Emplace(SampleTime, BoneIndexType, Transform);
	}

	void Reset()
	{
		CachedTransforms.Reset();
	}

	bool IsEmpty() const
	{
		return CachedTransforms.IsEmpty();
	}

private:
	TArray<FCachedTransform<FTransformType>, TInlineAllocator<64, TMemStackAllocator<>>> CachedTransforms;
};

#if ENABLE_DRAW_DEBUG
struct POSESEARCH_API FDebugDrawParams
{
	FDebugDrawParams(FAnimInstanceProxy* InAnimInstanceProxy, const FTransform& InRootMotionTransform, const UPoseSearchDatabase* InDatabase, EDebugDrawFlags InFlags = EDebugDrawFlags::None);
	FDebugDrawParams(const UWorld* InWorld, const USkinnedMeshComponent* InMesh, const FTransform& InRootMotionTransform, const UPoseSearchDatabase* InDatabase, EDebugDrawFlags InFlags = EDebugDrawFlags::None);

	const FSearchIndex* GetSearchIndex() const;
	const UPoseSearchSchema* GetSchema() const;

	FVector ExtractPosition(TConstArrayView<float> PoseVector, const UPoseSearchFeatureChannel_Position* Position) const;
	FVector ExtractPosition(TConstArrayView<float> PoseVector, float SampleTimeOffset, int8 SchemaBoneIdx = RootSchemaBoneIdx, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, int32 SamplingAttributeId = INDEX_NONE) const;

	FQuat ExtractRotation(TConstArrayView<float> PoseVector, float SampleTimeOffset, int8 SchemaBoneIdx = RootSchemaBoneIdx) const;

	const FTransform& GetRootTransform() const;

	void DrawLine(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, float Thickness = 0.f) const;
	void DrawPoint(const FVector& Position, const FColor& Color, float Thickness = 6.f) const;
	void DrawCircle(const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, float Thickness = 1.f) const;
	void DrawCentripetalCatmullRomSpline(TConstArrayView<FVector> Points, TConstArrayView<FColor> Colors, float Alpha, int32 NumSamplesPerSegment, float Thickness = 1.f) const;
	
	void DrawFeatureVector(TConstArrayView<float> PoseVector);
	void DrawFeatureVector(int32 PoseIdx);

private:
	bool CanDraw() const;

	FAnimInstanceProxy* AnimInstanceProxy = nullptr;
	const UWorld* World = nullptr;
	const USkinnedMeshComponent* Mesh = nullptr;
	const FTransform* RootMotionTransform = nullptr;
	const UPoseSearchDatabase* Database = nullptr;
	EDebugDrawFlags Flags = EDebugDrawFlags::None;
};

#endif // ENABLE_DRAW_DEBUG

// float buffer of features according to a UPoseSearchSchema layout. Used to build search queries at runtime
struct FCachedQuery
{
public:
	explicit FCachedQuery(const UPoseSearchSchema* InSchema);
	const UPoseSearchSchema* GetSchema() const { return Schema; }
	TArrayView<float> EditValues() { return Values; }
	TConstArrayView<float> GetValues() const { return Values; }

private:
	TStackAlignedArray<float> Values;
	
	// no need for a TWeakObjectPtr since it doesn't persist across multiple frames (same lifespan as FSearchContext)
	const UPoseSearchSchema* Schema;
};

// CachedChannels uses hashed unique identifiers to determine channels that can share feature vector data during the building of the query
struct FCachedChannel
{
	// no need for a TWeakObjectPtr since it doesn't persist across multiple frames (same lifespan as FSearchContext)
	const UPoseSearchFeatureChannel* Channel = nullptr;

	// index of the associated query in FSearchContext::CachedQueries
	int32 CachedQueryIndex = INDEX_NONE;
};

struct POSESEARCH_API FSearchContext
{
	FSearchContext(const UAnimInstance* InAnimInstance, const IPoseHistory* InHistory, TConstArrayView<const UObject*> InAssetsToConsider = TConstArrayView<const UObject*>(),
		float InDesiredPermutationTimeOffset = 0.f, const FPoseIndicesHistory* InPoseIndicesHistory = nullptr,
		const FSearchResult& InCurrentResult = FSearchResult(), const FFloatInterval& InPoseJumpThresholdTime = FFloatInterval(0.f, 0.f), bool bInUseCachedChannelData = false);

	// Returns the rotation of the bone Schema.BoneReferences[SchemaSampleBoneIdx] at an offset time of SampleTimeOffset relative to the
	// transform of the bone Schema.BoneReferences[SchemaOriginBoneIdx] at an offset time of time OriginTimeOffset 
	// Times will be processed by GetPermutationTimeOffsets(PermutationTimeType, ...)
	FQuat GetSampleRotation(float SampleTimeOffset, float OriginTimeOffset, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, const FQuat* SampleBoneRotationWorldOverride = nullptr);
	
	// Returns the position of the bone Schema.BoneReferences[SchemaSampleBoneIdx] at an offset time of SampleTimeOffset relative to the
	// transform of the bone Schema.BoneReferences[SchemaOriginBoneIdx] at an offset time of time OriginTimeOffset 
	// Times will be processed by GetPermutationTimeOffsets(PermutationTimeType, ...)
	FVector GetSamplePosition(float SampleTimeOffset, float OriginTimeOffset, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, const FVector* SampleBonePositionWorldOverride = nullptr);
	
	// Returns the delta velocity of the velocity of the bone Schema.BoneReferences[SchemaSampleBoneIdx] at an offset time of SampleTimeOffset minus
	// the velocity of the bone Schema.BoneReferences[SchemaOriginBoneIdx] at an offset time of time OriginTimeOffset 
	// Times will be processed by GetPermutationTimeOffsets(PermutationTimeType, ...)
	// if bUseCharacterSpaceVelocities is true, velocities will be computed in root bone space, rather than world space
	FVector GetSampleVelocity(float SampleTimeOffset, float OriginTimeOffset, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseCharacterSpaceVelocities = true, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, const FVector* SampleBoneVelocityWorldOverride = nullptr);

	void ResetCurrentBestCost();
	void UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost);
	float GetCurrentBestTotalCost() const { return CurrentBestTotalCost; }

	TConstArrayView<float> GetOrBuildQuery(const UPoseSearchSchema* Schema);
	TConstArrayView<float> GetCachedQuery(const UPoseSearchSchema* Schema) const;

	bool IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const;
	bool CanUseCurrentResult() const;

	TConstArrayView<float> GetCurrentResultPoseVector() const { return CurrentResultPoseVector; }

	void UpdateCurrentResultPoseVector();
	const FSearchResult& GetCurrentResult() const { return CurrentResult; }
	const FFloatInterval& GetPoseJumpThresholdTime() const { return PoseJumpThresholdTime; }
	const FPoseIndicesHistory* GetPoseIndicesHistory() const { return PoseIndicesHistory; }
	const IPoseHistory* GetHistory() const { return History; }
	float GetDesiredPermutationTimeOffset() const { return DesiredPermutationTimeOffset; }
	const UAnimInstance* GetAnimInstance() const { return AnimInstance; }

	void SetAssetsToConsider(TConstArrayView<const UObject*> InAssetsToConsider) { AssetsToConsider = InAssetsToConsider; }
	TConstArrayView<const UObject*> GetAssetsToConsider() const { return AssetsToConsider; }
	
	// returns the world space transform of the bone SchemaBoneIdx at time SampleTime
	FTransform GetWorldBoneTransformAtTime(float SampleTime, int8 SchemaBoneIdx = RootSchemaBoneIdx);
	
#if WITH_EDITOR
	void SetAsyncBuildIndexInProgress() { bAsyncBuildIndexInProgress = true; }
	void ResetAsyncBuildIndexInProgress() { bAsyncBuildIndexInProgress = false; }
	bool IsAsyncBuildIndexInProgress() const { return bAsyncBuildIndexInProgress; }
#endif // WITH_EDITOR

	bool AnyCachedQuery() const { return !CachedQueries.IsEmpty(); }
	void AddNewFeatureVectorBuilder(const UPoseSearchSchema* Schema) { CachedQueries.Emplace(Schema); }
	TArrayView<float> EditFeatureVector();
	
	const UPoseSearchFeatureChannel* GetCachedChannelData(uint32 ChannelUniqueIdentifier, const UPoseSearchFeatureChannel* Channel, TConstArrayView<float>& CachedChannelData);
	bool IsUseCachedChannelData() const { return bUseCachedChannelData; }
	void SetUseCachedChannelData(bool bInUseCachedChannelData) { bUseCachedChannelData = bInUseCachedChannelData; }

private:
	FVector GetSamplePositionInternal(float SampleTime, float OriginTime, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, const FVector* SampleBonePositionWorldOverride = nullptr);
	FQuat GetSampleRotationInternal(float SampleTime, float OriginTime, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, const FQuat* SampleBoneRotationWorldOverride = nullptr);
	FTransform GetWorldRootBoneTransformAtTime(float SampleTime) const;
	
	const UAnimInstance* AnimInstance = nullptr;
	const IPoseHistory* History = nullptr;

	// if AssetsToConsider is not empty, we'll search only for poses from UObject(s) that are in the AssetsToConsider
	TConstArrayView<const UObject*> AssetsToConsider;

	const float DesiredPermutationTimeOffset = 0.f;
	const FPoseIndicesHistory* PoseIndicesHistory = nullptr;
	const FSearchResult& CurrentResult;
	const FFloatInterval& PoseJumpThresholdTime;
	bool bUseCachedChannelData = false;

	TConstArrayView<float> CurrentResultPoseVector;

	// @todo: use a 16 bytes aligned TInlineAllocator with overflow TMemStackAllocator
	TStackAlignedArray<float> CurrentResultPoseVectorData;

	// transforms cached in world space
	FCachedTransforms<FTransform> CachedTransforms;

	TArray<FCachedQuery, TInlineAllocator<PreallocatedCachedQueriesNum, TMemStackAllocator<>>> CachedQueries;

	// @todo add an overflow TMemStackAllocator
	// mapping channel unique identifier (hash) to FCachedChannel
	TMap<uint32, FCachedChannel, TInlineSetAllocator<PreallocatedCachedChannelDataNum>> CachedChannels;

	float CurrentBestTotalCost = MAX_flt;
	
#if WITH_EDITOR
	bool bAsyncBuildIndexInProgress = false;
#endif // WITH_EDITOR

#if UE_POSE_SEARCH_TRACE_ENABLED

	struct FPoseCandidateIdCost
	{
		int32 PoseIdx = 0;
		FPoseSearchCost Cost;
		bool operator<(const FPoseCandidateIdCost& Other) const { return Other.Cost < Cost; } // Reverse compare because BestCandidates is a max heap
	};
	
public:

	struct FPoseCandidate : public FPoseCandidateIdCost
	{
		EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;
	};

	void Track(const UPoseSearchDatabase* Database, int32 PoseIdx = INDEX_NONE, EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None, const FPoseSearchCost& Cost = FPoseSearchCost())
	{
		check(Database);

		FBestPoseCandidates& BestPoseCandidates = BestPoseCandidatesMap.FindOrAdd(Database);
		if (PoseIdx != INDEX_NONE)
		{
			BestPoseCandidates.Add(PoseIdx, PoseCandidateFlags, Cost);
		}
	}


	struct FBestPoseCandidates
	{
		FBestPoseCandidates()
		{
			// preallocating memory to avoid multiple reallocations / rehashing
			PoseCandidateHeap.Reserve(MaxNumberOfCollectedPoseCandidatesPerDatabase);
			PoseIdxToFlags.Empty(MaxNumberOfCollectedPoseCandidatesPerDatabase);
		}

		void Add(int32 PoseIdx, EPoseCandidateFlags PoseCandidateFlags, const FPoseSearchCost& Cost)
		{
			check(PoseIdx >= 0);
			if (EPoseCandidateFlags* PoseIdxPoseCandidateFlags = PoseIdxToFlags.Find(PoseIdx))
			{
				*PoseIdxPoseCandidateFlags |= PoseCandidateFlags;
			}
			else if (PoseCandidateHeap.Num() < MaxNumberOfCollectedPoseCandidatesPerDatabase || Cost < PoseCandidateHeap.HeapTop().Cost)
			{
				bool bPoppedContinuingPoseCandidate = false;
				FPoseCandidate ContinuingPoseCandidate;
				while (PoseCandidateHeap.Num() >= MaxNumberOfCollectedPoseCandidatesPerDatabase)
				{
					FPoseCandidate PoppedPoseCandidate;
					Pop(PoppedPoseCandidate);

					if (EnumHasAnyFlags(PoppedPoseCandidate.PoseCandidateFlags, EPoseCandidateFlags::Valid_ContinuingPose))
					{
						// we can only have one continuing pose candidate
						check(!bPoppedContinuingPoseCandidate);
						ContinuingPoseCandidate = PoppedPoseCandidate;
						bPoppedContinuingPoseCandidate = true;
					}					
				}

				if (bPoppedContinuingPoseCandidate)
				{
					// if we popped the continuing pose candidate, we make some space for it and push it back
					FPoseCandidate PoppedPoseCandidate;
					Pop(PoppedPoseCandidate);
					PoseCandidateHeap.HeapPush(ContinuingPoseCandidate);
					PoseIdxToFlags.Add(ContinuingPoseCandidate.PoseIdx, ContinuingPoseCandidate.PoseCandidateFlags);
				}

				FPoseCandidate PoseCandidate;
				PoseCandidate.PoseIdx = PoseIdx;
				PoseCandidate.Cost = Cost;
				PoseCandidateHeap.HeapPush(PoseCandidate);
				PoseIdxToFlags.Add(PoseIdx, PoseCandidateFlags);
			}
		}

		int32 Num() const
		{
			return PoseCandidateHeap.Num();
		}

		FPoseCandidate GetUnsortedCandidate(int32 Index) const
		{
			FPoseCandidate PoseCandidate;
			const FPoseCandidateIdCost& PoseCandidateIdCost = PoseCandidateHeap[Index];
			PoseCandidate.PoseIdx = PoseCandidateIdCost.PoseIdx;
			PoseCandidate.Cost = PoseCandidateIdCost.Cost;
			PoseCandidate.PoseCandidateFlags = PoseIdxToFlags[PoseCandidateIdCost.PoseIdx];
			return PoseCandidate;
		}

	private:
		void Pop(FPoseCandidate& OutItem)
		{
			PoseCandidateHeap.HeapPop(OutItem, false);
			OutItem.PoseCandidateFlags = PoseIdxToFlags.FindAndRemoveChecked(OutItem.PoseIdx);
		}

		TArray<FPoseCandidateIdCost> PoseCandidateHeap;
		TMap<int32, EPoseCandidateFlags> PoseIdxToFlags;
	};
	
	const TMap<const UPoseSearchDatabase*, FBestPoseCandidates>& GetBestPoseCandidatesMap() const
	{
		return BestPoseCandidatesMap;
	}

private:
	TMap<const UPoseSearchDatabase*, FBestPoseCandidates> BestPoseCandidatesMap;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
};

} // namespace UE::PoseSearch
