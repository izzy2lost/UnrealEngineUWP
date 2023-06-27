// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearchEigenHelper.h"

namespace UE::PoseSearch
{

static FORCEINLINE float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());
	Eigen::Map<const Eigen::ArrayXf> VW(WeightsSqrt.GetData(), WeightsSqrt.Num());

	return ((VA - VB) * VW).square().sum();
}

static FORCEINLINE float CompareAlignedFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num());
	check(A.Num() % 4 == 0);
	check(IsAligned(A.GetData(), alignof(VectorRegister4Float)));
	check(IsAligned(B.GetData(), alignof(VectorRegister4Float)));
	check(IsAligned(WeightsSqrt.GetData(), alignof(VectorRegister4Float)));
	// sufficient condition to check for pointer overlapping
	check(A.GetData() != B.GetData() && A.GetData() != WeightsSqrt.GetData());

	const int32 NumVectors = A.Num() / 4;

	const VectorRegister4Float* RESTRICT VA = reinterpret_cast<const VectorRegister4Float*>(A.GetData());
	const VectorRegister4Float* RESTRICT VB = reinterpret_cast<const VectorRegister4Float*>(B.GetData());
	const VectorRegister4Float* RESTRICT VW = reinterpret_cast<const VectorRegister4Float*>(WeightsSqrt.GetData());

	VectorRegister4Float PartialCost = VectorZero();
	for (int32 VectorIdx = 0; VectorIdx < NumVectors; ++VectorIdx, ++VA, ++VB, ++VW)
	{
		const VectorRegister4Float Diff = VectorSubtract(*VA, *VB);
		const VectorRegister4Float WeightedDiff = VectorMultiply(Diff, *VW);
		PartialCost = VectorMultiplyAdd(WeightedDiff, WeightedDiff, PartialCost);
	}

	// calculating PartialCost.X + PartialCost.Y + PartialCost.Z + PartialCost.W
	VectorRegister4Float Swizzle = VectorSwizzle(PartialCost, 1, 0, 3, 2);	// (Y, X, W, Z) of PartialCost
	PartialCost = VectorAdd(PartialCost, Swizzle);							// (X + Y, Y + X, Z + W, W + Z)
	Swizzle = VectorSwizzle(PartialCost, 2, 3, 0, 1);						// (Z + W, W + Z, X + Y, Y + X)
	PartialCost = VectorAdd(PartialCost, Swizzle);							// (X + Y + Z + W, Y + X + W + Z, Z + W + X + Y, W + Z + Y + X)
	float Cost;
	VectorStoreFloat1(PartialCost, &Cost);

// keeping this debug code to validate CompareAlignedFeatureVectors against CompareFeatureVectors
//#if DO_CHECK
//	const float EigenCost = CompareFeatureVectors(A, B, WeightsSqrt);
//
//	if (!FMath::IsNearlyEqual(Cost, EigenCost))
//	{
//		const float RelativeDifference = Cost > EigenCost ? (Cost - EigenCost) / Cost : (EigenCost - Cost) / EigenCost;
//		check(FMath::IsNearlyZero(RelativeDifference, UE_KINDA_SMALL_NUMBER));
//	}
//#endif //DO_CHECK
	
	return Cost;
}

void CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt, TArrayView<float> Result)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num() && A.Num() == Result.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());
	Eigen::Map<const Eigen::ArrayXf> VW(WeightsSqrt.GetData(), WeightsSqrt.Num());
	Eigen::Map<Eigen::ArrayXf> VR(Result.GetData(), Result.Num());

	VR = ((VA - VB) * VW).square();
}

float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B)
{
	check(A.Num() == B.Num() && A.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());

	return (VA - VB).square().sum();
}

//////////////////////////////////////////////////////////////////////////
// FPoseMetadata
FArchive& operator<<(FArchive& Ar, FPoseMetadata& Metadata)
{
	// storing more data than necessary for now to avoid to deal with endiannes
	uint32 ValueOffset = Metadata.GetValueOffset();
	uint32 AssetIndex = Metadata.GetAssetIndex();
	bool bInBlockTransition = Metadata.IsBlockTransition();
	FFloat16 CostAddend = Metadata.CostAddend;
	
	Ar << ValueOffset;
	Ar << AssetIndex;
	Ar << bInBlockTransition;
	Ar << CostAddend;

	Metadata = FPoseMetadata(ValueOffset, AssetIndex, bInBlockTransition, CostAddend);
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FSearchIndexAsset
FArchive& operator<<(FArchive& Ar, FSearchIndexAsset& IndexAsset)
{
	Ar << IndexAsset.SourceAssetIdx;
	Ar << IndexAsset.bMirrored;
	Ar << IndexAsset.PermutationIdx;
	Ar << IndexAsset.BlendParameters;
	Ar << IndexAsset.FirstPoseIdx;
	Ar << IndexAsset.FirstSampleIdx;
	Ar << IndexAsset.LastSampleIdx;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FSearchStats
FArchive& operator<<(FArchive& Ar, FSearchStats& Stats)
{
	Ar << Stats.AverageSpeed;
	Ar << Stats.MaxSpeed;
	Ar << Stats.AverageAcceleration;
	Ar << Stats.MaxAcceleration;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchBaseIndex
const FSearchIndexAsset& FSearchIndexBase::GetAssetForPose(int32 PoseIdx) const
{
	const uint32 AssetIndex = PoseMetadata[PoseIdx].GetAssetIndex();
	return Assets[AssetIndex];
}

const FSearchIndexAsset* FSearchIndexBase::GetAssetForPoseSafe(int32 PoseIdx) const
{
	if (PoseMetadata.IsValidIndex(PoseIdx))
	{
		const uint32 AssetIndex = PoseMetadata[PoseIdx].GetAssetIndex();
		if (Assets.IsValidIndex(AssetIndex))
		{
			return &Assets[AssetIndex];
		}
	}
	return nullptr;
}

bool FSearchIndexBase::IsEmpty() const
{
	return Assets.IsEmpty() || PoseMetadata.IsEmpty();
}

void FSearchIndexBase::Reset()
{
	*this = FSearchIndexBase();
}

void FSearchIndexBase::PruneDuplicateValues(float SimilarityThreshold, int32 DataCardinality)
{
	constexpr bool bParalleEvaluation = true;

	const int32 NumPoses = GetNumPoses();
	if (SimilarityThreshold > 0.f && NumPoses >= 2)
	{
		struct FPosePair
		{
			int32 PoseIdxA = 0;
			int32 PoseIdxB = 0;
		};
		struct FPosePairSimilarity : public FPosePair
		{
			float Similarity = 0.f;
		};
		TArray<FPosePairSimilarity> PosePairSimilarities;
		PosePairSimilarities.Reserve(1024 * 64);

		// calculating a sparse proximity matrix with Similarity up to SimilarityThreshold (to perform something similar to hierarchical clustering)
		if (bParalleEvaluation)
		{
			// @todo: we should use a smarter approach, knowing we need NumPoses * (NumPoses - 1) / 2 comparisons, and reconstruct the pose indexes from the comparison index...
			
			// doing it in batches of items to avoid using too much memory
			constexpr int32 BatchSize = 1024 * 1024;

			TArray<FPosePair> PosePairs;
			PosePairs.Reserve(BatchSize);
			FPosePair PosePair;
			FCriticalSection Mutex;

			auto EvaluatePosePairs = [this, DataCardinality, SimilarityThreshold, &PosePairs, &PosePairSimilarities, &Mutex]()
			{
				ParallelFor(PosePairs.Num(), [this, DataCardinality, SimilarityThreshold, &PosePairs, &PosePairSimilarities, &Mutex](int32 ComparisonIndex)
				{
					const int PoseIdxA = PosePairs[ComparisonIndex].PoseIdxA;
					const int PoseIdxB = PosePairs[ComparisonIndex].PoseIdxB;
					TConstArrayView<float> ValuesA = GetPoseValuesBase(PoseIdxA, DataCardinality);
					TConstArrayView<float> ValuesB = GetPoseValuesBase(PoseIdxB, DataCardinality);
					const float Similarity = CompareFeatureVectors(ValuesA, ValuesB);
					if (Similarity < SimilarityThreshold)
					{
						// since this condition doesn't happen often, we're not gonna spend too much time in mutex contention
						FScopeLock Lock(&Mutex);
						FPosePairSimilarity PosePairSimilarity;
						PosePairSimilarity.PoseIdxA = PoseIdxA;
						PosePairSimilarity.PoseIdxB = PoseIdxB;
						PosePairSimilarity.Similarity = Similarity;
						PosePairSimilarities.Emplace(PosePairSimilarity);
					}
				});
			};

			for (PosePair.PoseIdxA = 0; PosePair.PoseIdxA < NumPoses; ++PosePair.PoseIdxA)
			{
				for (PosePair.PoseIdxB = PosePair.PoseIdxA + 1; PosePair.PoseIdxB < NumPoses; ++PosePair.PoseIdxB)
				{
					PosePairs.Add(PosePair);
					if (PosePairs.Num() == BatchSize)
					{
						EvaluatePosePairs();

						PosePairs.Reset();
					}
				}
			}

			EvaluatePosePairs();
		}
		else
		{
			for (int32 PoseIdxA = 0; PoseIdxA < NumPoses; ++PoseIdxA)
			{
				TConstArrayView<float> ValuesA = GetPoseValuesBase(PoseIdxA, DataCardinality);
				for (int32 PoseIdxB = PoseIdxA + 1; PoseIdxB < NumPoses; ++PoseIdxB)
				{
					TConstArrayView<float> ValuesB = GetPoseValuesBase(PoseIdxB, DataCardinality);
					const float Similarity = CompareFeatureVectors(ValuesA, ValuesB);
					if (Similarity < SimilarityThreshold)
					{
						FPosePairSimilarity PosePairSimilarity;
						PosePairSimilarity.PoseIdxA = PoseIdxA;
						PosePairSimilarity.PoseIdxB = PoseIdxB;
						PosePairSimilarity.Similarity = Similarity;
						PosePairSimilarities.Emplace(PosePairSimilarity);
					}
				}
			}
		}

		if (!PosePairSimilarities.IsEmpty())
		{
			PosePairSimilarities.Sort([](const FPosePairSimilarity& A, const FPosePairSimilarity& B)
			{
				return A.Similarity < B.Similarity;
			});

			// mapping between the one value offset and all the poses sharing it
			TMap<uint32, TArray<int32>> ValueOffsetToPoses;
			for (int32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
			{
				const uint32 ValueOffset = PoseMetadata[PoseIdx].GetValueOffset();
				// FindOrAdd to support the eventuality of having multiple poses already sharing the same value offset
				ValueOffsetToPoses.FindOrAdd(ValueOffset).Add(PoseIdx);
			}

			// at this point ValueOffsetToPoses is fully populated with all the possible value offset, and since we're not adding, but eventually removing entries we can just use the [] operator
			int32 LastValidPoseIdx = NumPoses - 1;
			for (int32 PosePairSimilarityIdx = 0; PosePairSimilarityIdx < PosePairSimilarities.Num(); ++PosePairSimilarityIdx)
			{
				const FPosePairSimilarity& PosePairSimilarity = PosePairSimilarities[PosePairSimilarityIdx];
				const uint32 ValueOffsetA = PoseMetadata[PosePairSimilarity.PoseIdxA].GetValueOffset();
				const uint32 ValueOffsetB = PoseMetadata[PosePairSimilarity.PoseIdxB].GetValueOffset();
				
				// if the two poses don't point already to the same value offset, we can remove one of them
				if (ValueOffsetA != ValueOffsetB)
				{
					// transferring all the poses associated to ValueOffsetB to ValueOffsetA
					TArray<int32>& PosesAtValueOffsetA = ValueOffsetToPoses[ValueOffsetA];
					TArray<int32>& PosesAtValueOffsetB = ValueOffsetToPoses[ValueOffsetB];

					for (int32 PoseAtValueOffsetB : PosesAtValueOffsetB)
					{
						PoseMetadata[PoseAtValueOffsetB].SetValueOffset(ValueOffsetA);
						PosesAtValueOffsetA.Add(PoseAtValueOffsetB);
					}

					// moving the LastValidPoseIdx values into the location ValueOffsetB, that we just free up
					const uint32 ValueOffsetLast = LastValidPoseIdx * DataCardinality;
					if (ValueOffsetB != ValueOffsetLast)
					{
						FMemory::Memcpy(&Values[ValueOffsetB], &Values[ValueOffsetLast], DataCardinality * sizeof(float));
						TArray<int32>& PosesAtValueOffsetLast = ValueOffsetToPoses[ValueOffsetLast];
						
						for (int32 PoseAtValueOffsetLast : PosesAtValueOffsetLast)
						{
							PoseMetadata[PoseAtValueOffsetLast].SetValueOffset(ValueOffsetB);
						}

						PosesAtValueOffsetB = PosesAtValueOffsetLast;
						PosesAtValueOffsetLast.Reset();
					}
					else
					{
						PosesAtValueOffsetB.Reset();
					}

					--LastValidPoseIdx;
				}
			}

			// resizing the Values array  
			Values.SetNum((LastValidPoseIdx + 1) * DataCardinality);
		}
	}
}

void FSearchIndexBase::AllocateData(int32 DataCardinality, int32 NumPoses)
{
	Values.Reset();
	PoseMetadata.Reset();

	Values.SetNumZeroed(DataCardinality * NumPoses);
	PoseMetadata.SetNumZeroed(NumPoses);
}

FArchive& operator<<(FArchive& Ar, FSearchIndexBase& Index)
{
	Ar << Index.Values;
	Ar << Index.PoseMetadata;
	Ar << Index.bAnyBlockTransition;
	Ar << Index.Assets;
	Ar << Index.MinCostAddend;
	Ar << Index.Stats;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FSearchIndex
FSearchIndex::FSearchIndex(const FSearchIndex& Other)
	: FSearchIndexBase(Other)
	, WeightsSqrt(Other.WeightsSqrt)
	, PCAValues(Other.PCAValues)
	, PCAProjectionMatrix(Other.PCAProjectionMatrix)
	, Mean(Other.Mean)
	, KDTree(Other.KDTree)
	, PCAExplainedVariance(Other.PCAExplainedVariance)
{
	check(!PCAValues.IsEmpty() || KDTree.DataSource.PointCount == 0);
	KDTree.DataSource.Data = PCAValues.IsEmpty() ? nullptr : PCAValues.GetData();
}

FSearchIndex& FSearchIndex::operator=(const FSearchIndex& Other)
{
	if (this != &Other)
	{
		this->~FSearchIndex();
		new(this)FSearchIndex(Other);
	}
	return *this;
}

void FSearchIndex::Reset()
{
	FSearchIndex Default;
	*this = Default;
}

TConstArrayView<float> FSearchIndex::GetPoseValues(int32 PoseIdx) const
{
	return GetPoseValuesBase(PoseIdx, WeightsSqrt.Num());
}

TConstArrayView<float> FSearchIndex::GetReconstructedPoseValues(int32 PoseIdx, TArrayView<float> BufferUsedForReconstruction) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAReconstruct);

	const int32 NumDimensions = WeightsSqrt.Num();
	const int32 NumPoses = GetNumPoses();
	check(PoseIdx >= 0 && PoseIdx < NumPoses&& NumDimensions > 0);
	check(BufferUsedForReconstruction.Num() == NumDimensions);

	const int32 NumberOfPrincipalComponents = PCAValues.Num() / NumPoses;
	check(NumPoses * NumberOfPrincipalComponents == PCAValues.Num());

	const RowMajorVectorMapConst MapWeightsSqrt(WeightsSqrt.GetData(), 1, NumDimensions);
	const ColMajorMatrixMapConst MapPCAProjectionMatrix(PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);
	const RowMajorVectorMapConst MapMean(Mean.GetData(), 1, NumDimensions);
	const RowMajorMatrixMapConst MapPCAValues(PCAValues.GetData(), NumPoses, NumberOfPrincipalComponents);

	const RowMajorVector ReciprocalWeightsSqrt = MapWeightsSqrt.cwiseInverse();
	const RowMajorVector WeightedReconstructedValues = MapPCAValues.row(PoseIdx) * MapPCAProjectionMatrix.transpose() + MapMean;

	RowMajorVectorMap ReconstructedPoseValues(BufferUsedForReconstruction.GetData(), 1, NumDimensions);
	ReconstructedPoseValues = WeightedReconstructedValues.array() * ReciprocalWeightsSqrt.array();

	return BufferUsedForReconstruction;
}

TConstArrayView<float> FSearchIndex::PCAProject(TConstArrayView<float> PoseValues, TArrayView<float> BufferUsedForProjection) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAProject);

	const int32 NumDimensions = WeightsSqrt.Num();
	const int32 NumberOfPrincipalComponents = PCAProjectionMatrix.Num() / NumDimensions;

	check(PCAProjectionMatrix.Num() > 0 && PCAProjectionMatrix.Num() % NumDimensions == 0);
	check(BufferUsedForProjection.Num() == NumberOfPrincipalComponents);

	const RowMajorVectorMapConst WeightsSqrtMap(WeightsSqrt.GetData(), 1, NumDimensions);
	const RowMajorVectorMapConst MeanMap(Mean.GetData(), 1, NumDimensions);
	const ColMajorMatrixMapConst PCAProjectionMatrixMap(PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);
	const RowMajorVectorMapConst PoseValuesMap(PoseValues.GetData(), 1, NumDimensions);

	RowMajorVectorMap WeightedPoseValuesMap((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	WeightedPoseValuesMap = PoseValuesMap.array() * WeightsSqrtMap.array();

	RowMajorVectorMap CenteredPoseValuesMap((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	CenteredPoseValuesMap.noalias() = WeightedPoseValuesMap - MeanMap;

	RowMajorVectorMap ProjectedPoseValuesMap(BufferUsedForProjection.GetData(), 1, NumberOfPrincipalComponents);
	ProjectedPoseValuesMap.noalias() = CenteredPoseValuesMap * PCAProjectionMatrixMap;

	return BufferUsedForProjection;
}

TArray<float> FSearchIndex::GetPoseValuesSafe(int32 PoseIdx) const
{
	TArray<float> PoseValues;
	if (PoseIdx >= 0 && PoseIdx < GetNumPoses())
	{
		if (IsValuesEmpty())
		{
			const int32 NumDimensions = WeightsSqrt.Num();
			PoseValues.SetNumUninitialized(NumDimensions);
			GetReconstructedPoseValues(PoseIdx, PoseValues);
		}
		else
		{
			PoseValues = GetPoseValues(PoseIdx);
		}
	}
	return PoseValues;
}

TConstArrayView<float> FSearchIndex::GetPCAPoseValues(int32 PoseIdx) const
{
	if (PCAValues.IsEmpty())
	{
		return TConstArrayView<float>();
	}

	const int32 NumDimensions = WeightsSqrt.Num();
	const int32 NumberOfPrincipalComponents = PCAProjectionMatrix.Num() / NumDimensions;

	check(PCAProjectionMatrix.Num() > 0 && PCAProjectionMatrix.Num() % NumDimensions == 0);
	check(PoseIdx >= 0 && PoseIdx < GetNumPoses() && NumDimensions > 0);

	const int32 ValueOffset = PoseIdx * NumberOfPrincipalComponents;
	return MakeArrayView(&PCAValues[ValueOffset], NumberOfPrincipalComponents);
}

FPoseSearchCost FSearchIndex::ComparePoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const
{
	// base dissimilarity cost representing how the associated PoseIdx differ, in a weighted way, from the query pose (QueryValues)
	const float DissimilarityCost = CompareFeatureVectors(PoseValues, QueryValues, WeightsSqrt);

	// cost addend associated to Schema->BaseCostBias or overriden by UAnimNotifyState_PoseSearchModifyCost
	const float NotifyAddend = PoseMetadata[PoseIdx].GetCostAddend();
	return FPoseSearchCost(DissimilarityCost, NotifyAddend, ContinuingPoseCostBias);
}

FPoseSearchCost FSearchIndex::CompareAlignedPoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const
{
	// base dissimilarity cost representing how the associated PoseIdx differ, in a weighted way, from the query pose (QueryValues)
	const float DissimilarityCost = CompareAlignedFeatureVectors(PoseValues, QueryValues, WeightsSqrt);

	// cost addend associated to Schema->BaseCostBias or overriden by UAnimNotifyState_PoseSearchModifyCost
	const float NotifyAddend = PoseMetadata[PoseIdx].GetCostAddend();
	return FPoseSearchCost(DissimilarityCost, NotifyAddend, ContinuingPoseCostBias);
}

FArchive& operator<<(FArchive& Ar, FSearchIndex& Index)
{
	Ar << static_cast<FSearchIndexBase&>(Index);

	Ar << Index.WeightsSqrt;
	Ar << Index.PCAValues;
	Ar << Index.PCAProjectionMatrix;
	Ar << Index.Mean;
	Ar << Index.PCAExplainedVariance;

	Serialize(Ar, Index.KDTree, Index.PCAValues.GetData());
	return Ar;
}

} // namespace UE::PoseSearch
