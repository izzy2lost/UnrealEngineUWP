// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeCellData.h"

int32 UWorldPartitionRuntimeCellData::StreamingSourceCacheEpoch = 0;

UWorldPartitionRuntimeCellData::UWorldPartitionRuntimeCellData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedMinSourcePriority(MAX_uint8)
	, bCachedWasRequestedByBlockingSource(false)
	, CachedMinSquareDistanceToBlockingSource(MAX_dbl)
	, CachedMinBlockOnSlowStreamingRatio(MAX_flt)
	, CachedMinSpatialSortingPriority(MAX_dbl)
	, CachedSourceInfoEpoch(MIN_int32)
	, ContentBounds(ForceInit)
	, Priority(0)
	, HierarchicalLevel(0)
{}

void UWorldPartitionRuntimeCellData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << DebugName;
}

void UWorldPartitionRuntimeCellData::ResetStreamingSourceInfo() const
{
	CachedMinSourcePriority = MAX_uint8;
	bCachedWasRequestedByBlockingSource = false;
	CachedMinSquareDistanceToBlockingSource = MAX_dbl;
	CachedMinBlockOnSlowStreamingRatio = MAX_flt;
	CachedMinSpatialSortingPriority = MAX_dbl;
	CachedSourceInfoEpoch = StreamingSourceCacheEpoch;	
}

void UWorldPartitionRuntimeCellData::AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const
{
	if (CachedSourceInfoEpoch != StreamingSourceCacheEpoch)
	{
		ResetStreamingSourceInfo();
		check(CachedSourceInfoEpoch == StreamingSourceCacheEpoch);
	}

	CachedMinSourcePriority = FMath::Min((uint8)Source.Priority, CachedMinSourcePriority);

	if (Source.bBlockOnSlowLoading)
	{
		bCachedWasRequestedByBlockingSource = true;

		const FVector CellToSource = SourceShape.GetCenter() - ContentBounds.GetClosestPointTo(SourceShape.GetCenter());
		const double CellToSourceSquareDistance = CellToSource.SizeSquared();
		CachedMinSquareDistanceToBlockingSource = FMath::Min(CellToSourceSquareDistance, CachedMinSquareDistanceToBlockingSource);

		const double BlockOnSlowStreamingRatio = FMath::Sqrt(CachedMinSquareDistanceToBlockingSource) / SourceShape.GetRadius();
		CachedMinBlockOnSlowStreamingRatio = FMath::Min(CachedMinBlockOnSlowStreamingRatio, BlockOnSlowStreamingRatio);
	}

	// Compute square distance from cell to source ratio
	const double SoureDistanceRatio = FMath::Clamp(ContentBounds.ComputeSquaredDistanceToPoint(SourceShape.GetCenter()) / FMath::Square(SourceShape.GetRadius()), 0.0f, 1.0f);

	// Compute cosine angle from cell to source direction ratio
	const FVector CellToSource = SourceShape.GetCenter() - ContentBounds.GetClosestPointTo(SourceShape.GetCenter());
	const float SourceCosAngle = ContentBounds.IsInsideOrOn(SourceShape.GetCenter()) ? -1.0f : (SourceShape.GetAxis() | CellToSource.GetSafeNormal());
	const float SourceCosAngleRatio = SourceCosAngle * 0.5f + 0.5f;

	// Compute final cell priority for this source
	static float CosAngleContribution = 0.4f;
	const double SortingPriority = SoureDistanceRatio * FMath::Cube(FMath::Pow(SourceCosAngleRatio, CosAngleContribution));

	// Update if lower
	CachedMinSpatialSortingPriority = FMath::Min(CachedMinSpatialSortingPriority, SortingPriority);
}

/**
 * Sorting criterias:
 *	- Highest priority affecting source (lowest to highest)
 *	- Cell hierarchical level (highest to lowest)
 -	- Cell custom priority (lowest to highest)
 *	- Cell distance and angle from source (lowest to highest)
 */
int32 UWorldPartitionRuntimeCellData::SortCompare(const UWorldPartitionRuntimeCellData* InOther) const
{
	int64 Result = (int32)CachedMinSourcePriority - (int32)InOther->CachedMinSourcePriority;

	if (!Result)
	{
		// Cell hierarchical level (highest to lowest)
		Result = InOther->HierarchicalLevel - HierarchicalLevel;

		if (!Result)
		{
			// Cell priority (lower value is higher prio)
			Result = Priority - InOther->Priority;			

			if (!Result)
			{
				Result = (CachedMinSpatialSortingPriority < InOther->CachedMinSpatialSortingPriority) ? -1 : 1;
			}
		}
	}

	return (int32)FMath::Clamp(Result, -1, 1);
}

const FBox& UWorldPartitionRuntimeCellData::GetContentBounds() const
{
	return ContentBounds;
}

FBox UWorldPartitionRuntimeCellData::GetCellBounds() const
{
	return ContentBounds;
}

FString UWorldPartitionRuntimeCellData::GetDebugName() const
{
	return DebugName.GetString();
}