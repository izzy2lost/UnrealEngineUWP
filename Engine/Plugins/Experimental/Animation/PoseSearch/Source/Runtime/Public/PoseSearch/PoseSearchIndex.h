// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/KDTree.h"
#include "PoseSearch/PoseSearchCost.h"
#include "PoseSearch/PoseSearchDefines.h"

namespace UE::PoseSearch
{

POSESEARCH_API void CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt, TArrayView<float> Result);
POSESEARCH_API float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B);

/**
 * This is kept for each pose in the search index along side the feature vector values and is used to influence the search.
 */
struct FPoseMetadata
{
private:
	enum { ValueOffsetNumBits = 27 };
	enum { AssetIndexNumBits = 20 };
	enum { BlockTransitionNumBits = 1 };

	uint32 ValueOffset : ValueOffsetNumBits = 0;
	uint32 AssetIndex : AssetIndexNumBits = 0;
	bool bBlockTransition : BlockTransitionNumBits = 0;
	FFloat16 CostAddend = 0.f;

public:
	FPoseMetadata(uint32 InValueOffset = 0, uint32 InAssetIndex = 0, bool bInBlockTransition = false, float InCostAddend = 0.f)
	: ValueOffset(InValueOffset)
	, AssetIndex(InAssetIndex)
	, bBlockTransition(bInBlockTransition)
	, CostAddend(InCostAddend)
	{
		// checking for overflowing inputs
		check(InValueOffset < (1 << ValueOffsetNumBits));
		check(InValueOffset < (1 << AssetIndexNumBits));
	}

	bool IsBlockTransition() const
	{
		return bBlockTransition;
	}

	uint32 GetAssetIndex() const
	{
		return AssetIndex;
	}

	float GetCostAddend() const
	{
		return CostAddend;
	}

	uint32 GetValueOffset() const
	{
		return ValueOffset;
	}

	void SetValueOffset(uint32 Value)
	{
		ValueOffset = Value;
	}

	friend FArchive& operator<<(FArchive& Ar, FPoseMetadata& Metadata);
};

/**
* Information about a source animation asset used by a search index.
* Some source animation entries may generate multiple FSearchIndexAsset entries.
**/
struct FSearchIndexAsset
{
	FSearchIndexAsset() {}

	FSearchIndexAsset(
		int32 InSourceAssetIdx,
		int32 InFirstPoseIdx,
		bool bInMirrored, 
		const FFloatInterval& InSamplingInterval,
		int32 SchemaSampleRate,
		int32 InPermutationIdx,
		FVector InBlendParameters = FVector::Zero())
		: SourceAssetIdx(InSourceAssetIdx)
		, bMirrored(bInMirrored)
		, PermutationIdx(InPermutationIdx)
		, BlendParameters(InBlendParameters)
		, FirstPoseIdx(InFirstPoseIdx)
		, FirstSampleIdx(FMath::CeilToInt(InSamplingInterval.Min * SchemaSampleRate))
		, LastSampleIdx(FMath::FloorToInt(InSamplingInterval.Max * SchemaSampleRate))
	{
		check(SchemaSampleRate > 0);
	}

	// Index of the source asset in search index's container (i.e. UPoseSearchDatabase)
	int32 SourceAssetIdx = INDEX_NONE;
	bool bMirrored = false;
	int32 PermutationIdx = INDEX_NONE;
	FVector BlendParameters = FVector::Zero();
	int32 FirstPoseIdx = INDEX_NONE;
	int32 FirstSampleIdx = INDEX_NONE;
	int32 LastSampleIdx = INDEX_NONE;

	bool IsPoseInRange(int32 PoseIdx) const { return (PoseIdx >= FirstPoseIdx) && (PoseIdx < FirstPoseIdx + GetNumPoses()); }

#if DO_CHECK
	bool IsInitialized() const 
	{
		return 
			SourceAssetIdx != INDEX_NONE &&
			PermutationIdx != INDEX_NONE &&
			FirstPoseIdx != INDEX_NONE &&
			FirstSampleIdx != INDEX_NONE &&
			LastSampleIdx != INDEX_NONE;
	}
#endif // DO_CHECK

	int32 GetBeginSampleIdx() const { return FirstSampleIdx; }
	int32 GetEndSampleIdx() const {	return LastSampleIdx + 1; }
	int32 GetNumPoses() const { return GetEndSampleIdx() - GetBeginSampleIdx(); }

	float GetFirstSampleTime(int32 SchemaSampleRate) const { check(SchemaSampleRate > 0); return FirstSampleIdx / float(SchemaSampleRate); }
	float GetLastSampleTime(int32 SchemaSampleRate) const { check(SchemaSampleRate > 0); return LastSampleIdx / float(SchemaSampleRate); }

	int32 GetPoseIndexFromTime(float Time, bool bIsLooping, int32 SchemaSampleRate) const
	{
		check(IsInitialized());

		const int32 NumPoses = GetNumPoses();
		int32 PoseOffset = FMath::RoundToInt(SchemaSampleRate * Time) - FirstSampleIdx;
		if (bIsLooping)
		{
			if (PoseOffset < 0)
			{
				PoseOffset = (PoseOffset % NumPoses) + NumPoses;
			}
			else if (PoseOffset >= NumPoses)
			{
				PoseOffset = PoseOffset % NumPoses;
			}
			return FirstPoseIdx + PoseOffset;
		}

		if (PoseOffset >= 0 && PoseOffset < NumPoses)
		{
			return FirstPoseIdx + PoseOffset;
		}

		return INDEX_NONE;
	}

	float GetTimeFromPoseIndex(int32 PoseIdx, int32 SchemaSampleRate) const
	{
		check(SchemaSampleRate > 0);

		const int32 PoseOffset = PoseIdx - FirstPoseIdx;
		check(PoseOffset >= 0 && PoseOffset < GetNumPoses());

		const float Time = (FirstSampleIdx + PoseOffset) / float(SchemaSampleRate);
		return Time;
	}

	friend FArchive& operator<<(FArchive& Ar, FSearchIndexAsset& IndexAsset);
};

struct FSearchStats
{
	float AverageSpeed = 0.f;
	float MaxSpeed = 0.f;
	float AverageAcceleration = 0.f;
	float MaxAcceleration = 0.f;
	friend FArchive& operator<<(FArchive& Ar, FSearchStats& Stats);
};

// compact representation of an array of arrays
template <typename Type = uint32>
struct FSparsePoseMultiMap
{
	FSparsePoseMultiMap(Type InMaxKey = Type(0), Type InMaxValue = Type(0))
	: MaxKey(InMaxKey)
	, MaxValue(InMaxValue)
	, DeltaKeyValue(InMaxValue >= InMaxKey ? InMaxValue - InMaxKey + 1 : 0)
	{
		// @todo: maybe expose this initial allocation budget
		DataValues.Reserve(InMaxKey * 2);
		for (Type Index = 0; Index < InMaxKey; ++Index)
		{
			DataValues.Add(Type(INDEX_NONE));
		}
	}

	void Insert(Type Key, TConstArrayView<Type> Values)
	{
#if DO_CHECK
		// key must be valid..
		check(Key != Type(INDEX_NONE));

		// ..and within range of acceptance
		check(Key >= 0 && Key < MaxKey);

		// DataValues[Key] should be empty - inserting the same key multiple times is not allowed
		check(DataValues[Key] == Type(INDEX_NONE));

		// Values must contains at least one element..
		check(!Values.IsEmpty());

		// ..and none of the elements should be an invalid value (or it'll confuse the key/value decoding)
		for (Type Value : Values)
		{
			check(Value <= MaxValue && Value != Type(INDEX_NONE));
		}
#endif //DO_CHECK

		// if Values contains only one element we store it directly at the location referenced by key
		if (Values.Num() == 1)
		{
			DataValues[Key] = Values[0];
		}
		// else we store the offset of the beginning of the encoded array (where the first element is the array size, followed by all the Values[i] elements
		else
		{
			// checking for overflow
			check((DataValues.Num() + 1 + Values.Num()) < (1 << (sizeof(Type) * 4 - 1)));
			check(int(MaxKey) <= DataValues.Num());

			// adding DeltaKeyValue to DataValues.Num() to making sure DataValues[Key] > MaxValue
			DataValues[Key] = DataValues.Num() + DeltaKeyValue;
			check(DataValues[Key] > MaxValue);

			// encoding Values at the end of DataValues, by storing its size.. 
			DataValues.Add(Values.Num());
			// ..and its data right after
			DataValues.Append(Values);
		}
	}

	TConstArrayView<Type> operator [](Type Key) const
	{
		check(Key != Type(INDEX_NONE) && Key < MaxKey);
		const Type Value = DataValues[Key];
		if (Value <= MaxValue)
		{
			return MakeArrayView(DataValues.GetData() + Key, 1);
		}

		check(Value >= DeltaKeyValue);
		const Type DecodedArrayStartLocation = Value - DeltaKeyValue;

		// decoding the array at location DecodedArrayStartLocation: its size is stored at DecodedArrayStartLocation offset..
		const Type Size = DataValues[DecodedArrayStartLocation];
		// ..and it's data starts at the next location DecodedArrayStartLocation + 1
		const Type DataOffset = DecodedArrayStartLocation + 1;
		check(int32(DataOffset + Size) <= DataValues.Num());
		return MakeArrayView(DataValues.GetData() + DataOffset, Size);
	}

	Type Num() const
	{
		return MaxKey;
	}

	SIZE_T GetAllocatedSize() const
	{
		return sizeof(MaxKey) + sizeof(MaxValue) + sizeof(DeltaKeyValue) + DataValues.GetAllocatedSize();
	}

	friend FArchive& operator<<(FArchive& Ar, FSparsePoseMultiMap& SparsePoseMultiMap)
	{
		Ar << SparsePoseMultiMap.MaxKey;
		Ar << SparsePoseMultiMap.MaxValue;
		Ar << SparsePoseMultiMap.DeltaKeyValue;
		Ar << SparsePoseMultiMap.DataValues;
		return Ar;
	}

	Type MaxKey = Type(0);
	Type MaxValue = Type(0);
	Type DeltaKeyValue = Type(0);
	TArray<Type> DataValues;
};

/**
* case class for FSearchIndex. building block used to gather data for data mining and calculate weights, pca, kdtree stuff
*/
struct FSearchIndexBase
{
	TAlignedArray<float> Values;
	TAlignedArray<FPoseMetadata> PoseMetadata;
	bool bAnyBlockTransition = false;
	TAlignedArray<FSearchIndexAsset> Assets;

	// minimum of the database metadata CostAddend: it represents the minimum cost of any search for the associated database (we'll skip the search in case the search result total cost is already less than MinCostAddend)
	float MinCostAddend = -MAX_FLT;

	// @todo: this property should be editor only
	FSearchStats Stats;

	int32 GetNumPoses() const { return PoseMetadata.Num(); }
	int32 GetNumValuesVectors(int32 DataCardinality) const
	{
		check(DataCardinality > 0);
		check(Values.Num() % DataCardinality == 0);
		return Values.Num() / DataCardinality;
	}

	bool IsValidPoseIndex(int32 PoseIdx) const { return PoseIdx < GetNumPoses(); }
	bool IsEmpty() const;
	bool IsValuesEmpty() const { return Values.IsEmpty(); }

	void ResetValues() { Values.Reset(); }
	void AllocateData(int32 DataCardinality, int32 NumPoses);
	
	const FSearchIndexAsset& GetAssetForPose(int32 PoseIdx) const;
	POSESEARCH_API const FSearchIndexAsset* GetAssetForPoseSafe(int32 PoseIdx) const;

	void Reset();
	
	void PruneDuplicateValues(float SimilarityThreshold, int32 DataCardinality);

	TConstArrayView<float> GetPoseValuesBase(int32 PoseIdx, int32 DataCardinality) const
	{
		check(!IsValuesEmpty() && PoseIdx >= 0 && PoseIdx < GetNumPoses());
		check(Values.Num() % DataCardinality == 0);
		const int32 ValueOffset = PoseMetadata[PoseIdx].GetValueOffset();
		return MakeArrayView(&Values[ValueOffset], DataCardinality);
	}

	friend FArchive& operator<<(FArchive& Ar, FSearchIndexBase& Index);
};

/**
* A search index for animation poses. The structure of the search index is determined by its UPoseSearchSchema.
* May represent a single animation (see UPoseSearchSequenceMetaData) or a collection (see UPoseSearchDatabase).
*/
struct FSearchIndex : public FSearchIndexBase
{
	// we store weights square roots to reduce numerical errors when CompareFeatureVectors 
	// ((VA - VB) * VW).square().sum()
	// instead of
	// ((VA - VB).square() * VW).sum()
	// since (VA - VB).square() could lead to big numbers, and VW being multiplied by the variance of the dataset
	TAlignedArray<float> WeightsSqrt;
	TAlignedArray<float> PCAValues;
	FSparsePoseMultiMap<uint32> PCAValuesVectorToPoseIndexes;
	TAlignedArray<float> PCAProjectionMatrix;
	TAlignedArray<float> Mean;

	FKDTree KDTree;

	// @todo: this property should be editor only
	float PCAExplainedVariance = 0.f;

	FSearchIndex() = default;
	~FSearchIndex() = default;
	FSearchIndex(const FSearchIndex& Other); // custom copy constructor to deal with the KDTree DataSrc
	FSearchIndex(FSearchIndex&& Other) = delete;
	FSearchIndex& operator=(const FSearchIndex& Other); // custom equal operator to deal with the KDTree DataSrc
	FSearchIndex& operator=(FSearchIndex&& Other) = delete;

	void Reset();
	TConstArrayView<float> GetPoseValues(int32 PoseIdx) const;
	TConstArrayView<float> GetReconstructedPoseValues(int32 PoseIdx, TArrayView<float> BufferUsedForReconstruction) const;
	POSESEARCH_API TConstArrayView<float> PCAProject(TConstArrayView<float> PoseValues, TArrayView<float> BufferUsedForProjection) const;

	POSESEARCH_API TArray<float> GetPoseValuesSafe(int32 PoseIdx) const;
	POSESEARCH_API TConstArrayView<float> GetPCAPoseValues(int32 PoseIdx) const;
	POSESEARCH_API FPoseSearchCost ComparePoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const;
	POSESEARCH_API FPoseSearchCost CompareAlignedPoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const;

	void PruneDuplicatePCAValues(float SimilarityThreshold, int32 NumberOfPrincipalComponents);

	friend FArchive& operator<<(FArchive& Ar, FSearchIndex& Index);
};

} // namespace UE::PoseSearch
