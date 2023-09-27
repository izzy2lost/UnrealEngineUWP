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

	// Draw using Query colors form the schema / config
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
	TArray<FCachedTransform<FTransformType>, TInlineAllocator<64>> CachedTransforms;
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

struct POSESEARCH_API FSearchContext
{
	FSearchContext(const UAnimInstance* InAnimInstance, const IPoseHistory* InHistory, TConstArrayView<const UAnimationAsset*> InAnimationsToConsider = TConstArrayView<const UAnimationAsset*>(),
		const FPoseSearchQueryTrajectory* InTrajectory = nullptr, float InDesiredPermutationTimeOffset = 0.f, const FPoseIndicesHistory* InPoseIndicesHistory = nullptr,
		const FSearchResult& InCurrentResult = FSearchResult(), const FFloatInterval& InPoseJumpThresholdTime = FFloatInterval(0.f, 0.f), bool bInForceInterrupt = false);

	// Returns the rotation of the bone Schema.BoneReferences[SchemaSampleBoneIdx] at an offset time of SampleTimeOffset relative to the
	// transform of the bone Schema.BoneReferences[SchemaOriginBoneIdx] at an offset time of time OriginTimeOffset 
	// Times will be processed by GetPermutationTimeOffsets(PermutationTimeType, ...)
	// if bUseHistoryRoot is true, eventually required root transforms will be gathered from the pose history node data, otherwise the context trajectory will be used
	FQuat GetSampleRotation(float SampleTimeOffset, float OriginTimeOffset, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, const FQuat* SampleBoneRotationWorldOverride = nullptr);
	
	// Returns the position of the bone Schema.BoneReferences[SchemaSampleBoneIdx] at an offset time of SampleTimeOffset relative to the
	// transform of the bone Schema.BoneReferences[SchemaOriginBoneIdx] at an offset time of time OriginTimeOffset 
	// Times will be processed by GetPermutationTimeOffsets(PermutationTimeType, ...)
	// if bUseHistoryRoot is true, eventually required root transforms will be gathered from the pose history node data, otherwise the context trajectory will be used
	FVector GetSamplePosition(float SampleTimeOffset, float OriginTimeOffset, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, const FVector* SampleBonePositionWorldOverride = nullptr);
	
	// Returns the delta velocity of the velocity of the bone Schema.BoneReferences[SchemaSampleBoneIdx] at an offset time of SampleTimeOffset minus
	// the velocity of the bone Schema.BoneReferences[SchemaOriginBoneIdx] at an offset time of time OriginTimeOffset 
	// Times will be processed by GetPermutationTimeOffsets(PermutationTimeType, ...)
	// if bUseCharacterSpaceVelocities is true, velocities will be computed in root bone space, rather than world space
	// if bUseHistoryRoot is true, eventually required root transforms will be gathered from the pose history node data, otherwise the context trajectory will be used
	FVector GetSampleVelocity(float SampleTimeOffset, float OriginTimeOffset, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseCharacterSpaceVelocities = true, bool bUseHistoryRoot = false, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime, const FVector* SampleBoneVelocityWorldOverride = nullptr);

	void ClearCachedEntries();

	void ResetCurrentBestCost();
	void UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost);
	float GetCurrentBestTotalCost() const { return CurrentBestTotalCost; }

	const FFeatureVectorBuilder& GetOrBuildQuery(const UPoseSearchSchema* Schema);
	const FFeatureVectorBuilder* GetCachedQuery(const UPoseSearchSchema* Schema) const;

	bool IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const;

	TConstArrayView<float> GetCurrentResultPoseVector() const { return CurrentResultPoseVector; }

	const FSearchResult& GetCurrentResult() const { return CurrentResult; }
	const FFloatInterval& GetPoseJumpThresholdTime() const { return PoseJumpThresholdTime; }
	const FPoseIndicesHistory* GetPoseIndicesHistory() const { return PoseIndicesHistory; }
	bool IsHistoryValid() const { return History != nullptr; }
	bool IsTrajectoryValid() const { return Trajectory != nullptr; }
	float GetDesiredPermutationTimeOffset() const { return DesiredPermutationTimeOffset; }
	bool IsForceInterrupt() const { return bForceInterrupt; }
	FTransform GetWorldRootBoneTransformAtTime(float SampleTime, bool bUseHistoryRoot = false, bool bExtrapolate = true) const;
	const UAnimInstance* GetAnimInstance() const { return AnimInstance; }

	void SetAnimationsToConsider(TConstArrayView<const UAnimationAsset*> InAnimationsToConsider) { AnimationsToConsider = InAnimationsToConsider; }
	TConstArrayView<const UAnimationAsset*> GetAnimationsToConsider() const { return AnimationsToConsider; }
	
private:
	// returns the world space transform of the bone SchemaBoneIdx at time SampleTime
	FTransform GetWorldBoneTransformAtTime(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false);
	
	// returns the local space transform relative to the root bone of the bone SchemaBoneIdx at time SampleTime
	FTransform GetLocalBoneTransformAtTime(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx);
	
	FVector GetSamplePositionInternal(float SampleTime, float OriginTime, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false, const FVector* SampleBonePositionWorldOverride = nullptr);
	FQuat GetSampleRotationInternal(float SampleTime, float OriginTime, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false, const FQuat* SampleBoneRotationWorldOverride = nullptr);

	const UAnimInstance* AnimInstance = nullptr;
	const IPoseHistory* History = nullptr;

	// if AnimationsToConsider is not empty, we'll search only for poses from UAnimationAsset(s) that are in the AnimationsToConsider
	TConstArrayView<const UAnimationAsset*> AnimationsToConsider;

	// Trajectory has been transformed in root bone world space reference system
	const FPoseSearchQueryTrajectory* Trajectory = nullptr;
	float DesiredPermutationTimeOffset = 0.f;
	const FPoseIndicesHistory* PoseIndicesHistory = nullptr;
	FSearchResult CurrentResult;
	FFloatInterval PoseJumpThresholdTime = FFloatInterval(0.f, 0.f);
	bool bForceInterrupt = false;

	TConstArrayView<float> CurrentResultPoseVector;
	TStackAlignedArray<float> CurrentResultPoseVectorData;

	// transforms cached in component space
	FCachedTransforms<FTransform> CachedTransforms;
	TArray<FFeatureVectorBuilder, TInlineAllocator<PreallocatedCachedQueriesNum>> CachedQueries;

	float CurrentBestTotalCost = MAX_flt;
	
#if UE_POSE_SEARCH_TRACE_ENABLED

	struct FPoseCandidateId
	{
		int32 PoseIdx = 0;
		const UPoseSearchDatabase* Database = nullptr;

		bool operator==(const FPoseCandidateId& Other) const
		{
			return PoseIdx == Other.PoseIdx && Database == Other.Database;
		}
		
		friend uint32 GetTypeHash(const FPoseCandidateId& PoseCandidateId)
		{
			return HashCombineFast(GetTypeHash(PoseCandidateId.PoseIdx), GetTypeHash(PoseCandidateId.Database));
		}
	};

	struct FPoseCandidateIdCost : public FPoseCandidateId
	{
		FPoseSearchCost Cost;
		bool operator<(const FPoseCandidateIdCost& Other) const { return Other.Cost < Cost; } // Reverse compare because BestCandidates is a max heap
	};
	
public:

	struct FPoseCandidate : public FPoseCandidateIdCost
	{
		EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;
	};

	struct FBestPoseCandidates
	{
		FBestPoseCandidates()
		{
			// preallocating memory to avoid multiple reallocations / rehashing
			PoseCandidateHeap.Reserve(MaxPoseCandidates);
			PoseIdxToFlags.Empty(MaxPoseCandidates);
		}

		void Add(const FPoseSearchCost& Cost, int32 PoseIdx, const UPoseSearchDatabase* Database, EPoseCandidateFlags PoseCandidateFlags)
		{
			FPoseCandidate PoseCandidate;
			PoseCandidate.PoseIdx = PoseIdx;
			PoseCandidate.Database = Database;

			if (EPoseCandidateFlags* PoseIdxPoseCandidateFlags = PoseIdxToFlags.Find(PoseCandidate))
			{
				*PoseIdxPoseCandidateFlags |= PoseCandidateFlags;
			}
			else if (PoseCandidateHeap.Num() < MaxPoseCandidates || Cost < PoseCandidateHeap.HeapTop().Cost)
			{
				bool bPoppedContinuingPoseCandidate = false;
				FPoseCandidate ContinuingPoseCandidate;
				while (PoseCandidateHeap.Num() >= MaxPoseCandidates)
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
					PoseIdxToFlags.Add(ContinuingPoseCandidate, ContinuingPoseCandidate.PoseCandidateFlags);
				}

				PoseCandidate.Cost = Cost;
				PoseCandidateHeap.HeapPush(PoseCandidate);
				PoseIdxToFlags.Add(PoseCandidate, PoseCandidateFlags);
			}
		}

		void Pop(FPoseCandidate& OutItem)
		{
			PoseCandidateHeap.HeapPop(OutItem, false);
			OutItem.PoseCandidateFlags = PoseIdxToFlags.FindAndRemoveChecked(OutItem);
		}

		bool IsEmpty() const
		{
			return PoseCandidateHeap.IsEmpty();
		}

		void SetMaxPoseCandidates(int32 Value)
		{
			MaxPoseCandidates = Value;
		}

	private:
		TArray<FPoseCandidateIdCost> PoseCandidateHeap;
		TMap<FPoseCandidateId, EPoseCandidateFlags> PoseIdxToFlags;
		int32 MaxPoseCandidates = 200;
	};
	
	FBestPoseCandidates BestCandidates;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
};

POSESEARCH_API FTransform MirrorTransform(const FTransform& InTransform, EAxis::Type MirrorAxis, const FQuat& ReferenceRotation);

} // namespace UE::PoseSearch
