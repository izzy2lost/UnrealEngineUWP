// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Chaos/Convex.h"
#include "Chaos/GJK.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Templates/Sorting.h"

#include "Misc/ScopeLock.h"


// Note: This anonymous namespace is copied/adapted from the experimental GeometryProcessing plugin so that it can be used here in a non-plugin context; it is backported from 5.0, where the geometry code is no longer in a plugin
//		 Specifically, TPointHashGrid3 and TScaleGridIndexer3, below, are from the GeometryProcessing code
namespace {

using FVector3i = Chaos::TVec3<int32>;

/**
 * Convert between integer grid coordinates and scaled real-valued coordinates (ie assumes integer grid origin == real origin)
 */
template<typename RealType>
struct TScaleGridIndexer3
{
	/** Real-valued size of an integer grid cell */
	RealType CellSize;

	TScaleGridIndexer3() : CellSize((RealType)1)
	{
		ensure(CellSize >= 1e-6);
	}

	TScaleGridIndexer3(RealType CellSize) : CellSize(CellSize)
	{
	}

	/** Convert real-valued point to integer grid coordinates */
	inline FVector3i ToGrid(const Chaos::TVec3<RealType>& P) const
	{
		return FVector3i(
			(int)FMath::Floor(P.X / CellSize),
			(int)FMath::Floor(P.Y / CellSize),
			(int)FMath::Floor(P.Z / CellSize));
	}

	/** Convert integer grid coordinates to real-valued point */
	inline Chaos::TVec3<RealType> FromGrid(const FVector3i& GridPoint) const
	{
		return Chaos::TVec3<RealType>(GridPoint.X * CellSize, GridPoint.Y * CellSize, GridPoint.Z * CellSize);
	}
};
/**
 * Hash Grid for values associated with 3D points.
 *
 * This class addresses the situation where you have a list of (point, point_data) and you
 * would like to be able to do efficient proximity queries, i.e. find the nearest point_data
 * for a given query point.
 *
 * We don't store copies of the 3D points. You provide a point_data type. This could just be the
 * integer index into your list for example, a pointer to something more complex, etc.
 * Insert and Remove functions require you to pass in the 3D point for the point_data.
 * To Update a point you need to know its old and new 3D coordinates.
 */
template<typename PointDataType, typename RealType>
class TPointHashGrid3
{
private:
	TMultiMap<FVector3i, PointDataType> Hash;
	FCriticalSection CriticalSection;
	TScaleGridIndexer3<RealType> Indexer;
	PointDataType InvalidValue;

public:

	/**
	 * Construct 3D hash grid
	 * @param cellSize size of grid cells
	 * @param InvalidValue this value will be returned by queries if no valid result is found (e.g. bounded-distance query)
	 */
	TPointHashGrid3(RealType cellSize, PointDataType InvalidValue) : Indexer(cellSize), InvalidValue(InvalidValue)
	{
	}

	/** Invalid grid value */
	PointDataType GetInvalidValue() const
	{
		return InvalidValue;
	}

	/**
	 * Insert at given position. This function is thread-safe.
	 * @param Value the point/value to insert
	 * @param Position the position associated with this value
	 */
	void InsertPoint(const PointDataType& Value, const Chaos::TVec3<RealType>& Position)
	{
		FVector3i idx = Indexer.ToGrid(Position);
		{
			FScopeLock Lock(&CriticalSection);
			Hash.Add(idx, Value);
		}
	}

	/**
	 * Insert at given position, without locking / thread-safety
	 * @param Value the point/value to insert
	 * @param Position the position associated with this value
	 */
	void InsertPointUnsafe(const PointDataType& Value, const Chaos::TVec3<RealType>& Position)
	{
		FVector3i idx = Indexer.ToGrid(Position);
		Hash.Add(idx, Value);
	}


	/**
	 * Remove at given position. This function is thread-safe.
	 * @param Value the point/value to remove
	 * @param Position the position associated with this value
	 * @return true if the value existed at this position
	 */
	bool RemovePoint(const PointDataType& Value, const Chaos::TVec3<RealType>& Position)
	{
		FVector3i idx = Indexer.ToGrid(Position);
		{
			FScopeLock Lock(&CriticalSection);
			return Hash.RemoveSingle(idx, Value) > 0;
		}
	}

	/**
	 * Remove at given position, without locking / thread-safety
	 * @param Value the point/value to remove
	 * @param Position the position associated with this value
	 * @return true if the value existed at this position
	 */
	bool RemovePointUnsafe(const PointDataType& Value, const Chaos::TVec3<RealType>& Position)
	{
		FVector3i idx = Indexer.ToGrid(Position);
		return Hash.RemoveSingle(idx, Value) > 0;
	}


	/**
	 * Test if the cell containing Position is empty. This function is thread-safe.
	 * Can be used to skip a more expensive range search, in some cases.
	 *
	 * @return true if the cell containing Position is empty
	 */
	bool IsCellEmpty(const Chaos::TVec3<RealType>& Position)
	{
		FVector3i Idx = Indexer.ToGrid(Position);
		{
			FScopeLock Lock(&CriticalSection);
			return !Hash.Contains(Idx);
		}
	}


	/**
	 * Test if the whole cell containing Position is empty, without locking / thread-safety
	 * Can be used to skip a more expensive range search, in some cases.
	 *
	 * @return true if the cell containing Position is empty
	 */
	bool IsCellEmptyUnsafe(const Chaos::TVec3<RealType>& Position)
	{
		FVector3i Idx = Indexer.ToGrid(Position);
		return !Hash.Contains(Idx);
	}


	/**
	 * Move value from old to new position. This function is thread-safe.
	 * @param Value the point/value to update
	 * @param OldPosition the current position associated with this value
	 * @param NewPosition the new position for this value
	 */
	void UpdatePoint(const PointDataType& Value, const Chaos::TVec3<RealType>& OldPosition, const Chaos::TVec3<RealType>& NewPosition)
	{
		FVector3i old_idx = Indexer.ToGrid(OldPosition);
		FVector3i new_idx = Indexer.ToGrid(NewPosition);
		if (old_idx == new_idx)
		{
			return;
		}
		bool bWasAtOldPos;
		{
			FScopeLock Lock(&CriticalSection);
			bWasAtOldPos = Hash.RemoveSingle(old_idx, Value) == 1;
		}
		check(bWasAtOldPos);
		{
			FScopeLock Lock(&CriticalSection);
			Hash.Add(new_idx, Value);
		}
		return;
	}


	/**
	 * Move value from old to new position, without locking / thread-safety
	 * @param Value the point/value to update
	 * @param OldPosition the current position associated with this value
	 * @param NewPosition the new position for this value
	 */
	void UpdatePointUnsafe(const PointDataType& Value, const Chaos::TVec3<RealType>& OldPosition, const Chaos::TVec3<RealType>& NewPosition)
	{
		FVector3i old_idx = Indexer.ToGrid(OldPosition);
		FVector3i new_idx = Indexer.ToGrid(NewPosition);
		if (old_idx == new_idx)
		{
			return;
		}
		bool bWasAtOldPos = Hash.RemoveSingle(old_idx, Value);
		check(bWasAtOldPos);
		Hash.Add(new_idx, Value);
		return;
	}


	/**
	 * Find nearest point in grid, within a given sphere, without locking / thread-safety.
	 * @param QueryPoint the center of the query sphere
	 * @param Radius the radius of the query sphere
	 * @param DistanceSqFunc Function you provide which measures the squared distance between QueryPoint and a Value
	 * @param IgnoreFunc optional Function you may provide which will result in a Value being ignored if IgnoreFunc(Value) returns true
	 * @return the found pair (Value,DistanceSqFunc(Value)), or (InvalidValue,MaxDouble) if not found
	 */
	TPair<PointDataType, RealType> FindNearestInRadius(
		const Chaos::TVec3<RealType>& QueryPoint, RealType Radius,
		TFunctionRef<RealType(const PointDataType&)> DistanceSqFunc,
		TFunctionRef<bool(const PointDataType&)> IgnoreFunc = [](const PointDataType& data) { return false; }) const
	{
		if (!Hash.Num())
		{
			return TPair<PointDataType, RealType>(GetInvalidValue(), TNumericLimits<RealType>::Max());
		}

		FVector3i min_idx = Indexer.ToGrid(QueryPoint - Radius * Chaos::TVec3<RealType>(1, 1, 1));
		FVector3i max_idx = Indexer.ToGrid(QueryPoint + Radius * Chaos::TVec3<RealType>(1, 1, 1));

		RealType min_distsq = TNumericLimits<RealType>::Max();
		PointDataType nearest = GetInvalidValue();
		RealType RadiusSquared = Radius * Radius;

		TArray<PointDataType> Values;
		for (int zi = min_idx.Z; zi <= max_idx.Z; zi++)
		{
			for (int yi = min_idx.Y; yi <= max_idx.Y; yi++)
			{
				for (int xi = min_idx.X; xi <= max_idx.X; xi++)
				{
					FVector3i idx(xi, yi, zi);
					Values.Reset();
					Hash.MultiFind(idx, Values);
					for (PointDataType Value : Values)
					{
						if (IgnoreFunc(Value))
						{
							continue;
						}
						RealType distsq = DistanceSqFunc(Value);
						if (distsq < RadiusSquared && distsq < min_distsq)
						{
							nearest = Value;
							min_distsq = distsq;
						}
					}
				}
			}
		}

		return TPair<PointDataType, RealType>(nearest, min_distsq);
	}


	/**
	 * Find all points in grid within a given sphere, without locking / thread-safety.
	 * @param QueryPoint the center of the query sphere
	 * @param Radius the radius of the query sphere
	 * @param DistanceSqFunc Function you provide which measures the squared distance between QueryPoint and a Value
	 * @param ResultsOut Array of output points in sphere
	 * @param IgnoreFunc optional Function you may provide which will result in a Value being ignored if IgnoreFunc(Value) returns true
	 * @return the number of found points
	 */
	int FindPointsInBall(
		const Chaos::TVec3<RealType>& QueryPoint, RealType Radius,
		TFunctionRef<RealType(const PointDataType&)> DistanceSqFunc,
		TArray<PointDataType>& ResultsOut,
		TFunctionRef<bool(const PointDataType&)> IgnoreFunc = [](const PointDataType& data) { return false; }) const
	{
		if (!Hash.Num())
		{
			return 0;
		}

		FVector3i min_idx = Indexer.ToGrid(QueryPoint - Radius * Chaos::TVec3<RealType>::One());
		FVector3i max_idx = Indexer.ToGrid(QueryPoint + Radius * Chaos::TVec3<RealType>::One());

		RealType RadiusSquared = Radius * Radius;

		TArray<PointDataType> Values;
		for (int zi = min_idx.Z; zi <= max_idx.Z; zi++)
		{
			for (int yi = min_idx.Y; yi <= max_idx.Y; yi++)
			{
				for (int xi = min_idx.X; xi <= max_idx.X; xi++)
				{
					FVector3i idx(xi, yi, zi);
					Values.Reset();
					Hash.MultiFind(idx, Values);
					for (PointDataType Value : Values)
					{
						if (IgnoreFunc(Value))
						{
							continue;
						}
						RealType distsq = DistanceSqFunc(Value);
						if (distsq < RadiusSquared)
						{
							ResultsOut.Add(Value);
						}
					}
				}
			}
		}

		return ResultsOut.Num();
	}
};
} // end of anonymous namespace / code copied from geometry plugin

FGeometryCollectionConvexUtility::FGeometryCollectionConvexData FGeometryCollectionConvexUtility::GetValidConvexHullData(FGeometryCollection* GeometryCollection)
{
	check (GeometryCollection)

	if (!GeometryCollection->HasGroup("Convex"))
	{
		GeometryCollection->AddGroup("Convex");
	}

	if (!GeometryCollection->HasAttribute("TransformToConvexIndices", FTransformCollection::TransformGroup))
	{
		FManagedArrayCollection::FConstructionParameters ConvexDependency("Convex");
		GeometryCollection->AddAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup, ConvexDependency);
	}

	if (!GeometryCollection->HasAttribute("ConvexHull", "Convex"))
	{
		GeometryCollection->AddAttribute<TUniquePtr<Chaos::FConvex>>("ConvexHull", "Convex");
	}

	// Check for correct population. Make sure all rigid nodes should have a convex associated; leave convex hulls for transform nodes alone for now
	const TManagedArray<int32>& SimulationType = GeometryCollection->GetAttribute<int32>("SimulationType", FTransformCollection::TransformGroup);
	const TManagedArray<int32>& TransformToGeometryIndex = GeometryCollection->GetAttribute<int32>("TransformToGeometryIndex", FTransformCollection::TransformGroup);
	TManagedArray<TSet<int32>>& TransformToConvexIndices = GeometryCollection->GetAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup);
	TManagedArray<TUniquePtr<Chaos::FConvex>>& ConvexHull = GeometryCollection->GetAttribute<TUniquePtr<Chaos::FConvex>>("ConvexHull", "Convex");
	
	TArray<int32> ProduceConvexHulls;
	ProduceConvexHulls.Reserve(SimulationType.Num());

	for (int32 Idx = 0; Idx < SimulationType.Num(); ++Idx)
	{
		if ((SimulationType[Idx] == FGeometryCollection::ESimulationTypes::FST_Rigid) && (TransformToConvexIndices[Idx].Num() == 0))
		{
			ProduceConvexHulls.Add(Idx);
		}
	}

	if (ProduceConvexHulls.Num())
	{
		int32 NewConvexIndexStart = GeometryCollection->AddElements(ProduceConvexHulls.Num(), "Convex");
		for (int32 Idx = 0; Idx < ProduceConvexHulls.Num(); ++Idx)
		{
			int32 GeometryIdx = TransformToGeometryIndex[ProduceConvexHulls[Idx]];
			ConvexHull[NewConvexIndexStart + Idx] = FindConvexHull(GeometryCollection, GeometryIdx);
			TransformToConvexIndices[ProduceConvexHulls[Idx]].Reset();
			TransformToConvexIndices[ProduceConvexHulls[Idx]].Add(NewConvexIndexStart + Idx);
		}
	}

	return { TransformToConvexIndices, ConvexHull };
}



namespace
{

typedef Chaos::TPlaneConcrete<Chaos::FReal, 3> FChaosPlane;

// Try to pick 4 points at the extremes of the shape, s.t. they enclose a non-zero volume.  Return the number of points picked.
// Concept from ConvexHull3.cpp code, which cannot be used here because it's in a plugin (this version is also slightly simpler / doesn't worry about the resulting tetrahedron's orientation, point filtering, etc)
int32 PickExtremePoints(const TArray<Chaos::FVec3>& InPts, int32 ExtremesOut[4])
{
	int32 NumPts = InPts.Num();

	if (NumPts < 4)
	{
		for (int32 Idx = 0; Idx < NumPts; Idx++)
		{
			ExtremesOut[Idx] = Idx;
		}
		return NumPts;
	}

	Chaos::FVec3 MinVal = InPts[0], MaxVal = InPts[0];
	FVector3i MinIdx(0,0,0), MaxIdx(0,0,0);
	for (int32 Idx = 1; Idx < NumPts; Idx++)
	{
		for (int32 Dim = 0; Dim < 3; Dim++)
		{
			Chaos::FReal Val = InPts[Idx][Dim];
			if (Val < MinVal[Dim])
			{
				MinIdx[Dim] = Idx;
				MinVal[Dim] = Val;
			}
			if (Val > MaxVal[Dim])
			{
				MaxIdx[Dim] = Idx;
				MaxVal[Dim] = Val;
			}
		}
	}
	int BestDim = 0;
	Chaos::FReal BestDimDiff = MaxVal[0] - MinVal[0];
	for (int32 Dim = 1; Dim < 3; Dim++)
	{
		Chaos::FReal DimDiff = MaxVal[0] - MinVal[0];
		if (DimDiff > BestDimDiff)
		{
			BestDimDiff = DimDiff;
			BestDim = Dim;
		}
	}

	ExtremesOut[0] = MinIdx[BestDim];
	ExtremesOut[1] = MaxIdx[BestDim];

	if (ExtremesOut[1] == ExtremesOut[0])
	{
		return 1;
	}

	Chaos::FVec3 LineStart = InPts[ExtremesOut[0]];
	Chaos::FVec3 LineDir = InPts[ExtremesOut[1]] - LineStart;
	if (!LineDir.Normalize())
	{
		return 1;
	}

	Chaos::FReal BestDSq = -1;
	Chaos::FVec3 BasisVec;
	int32 AwayFromLineIdx = 0;
	for (int32 Idx = 0; Idx < NumPts; Idx++)
	{
		Chaos::FVec3 Pt = InPts[Idx];
		Chaos::FVec3 OnLine = (Pt - LineStart).ProjectOnToNormal(LineDir) + LineStart;
		Chaos::FVec3 ToPt = Pt - OnLine;
		Chaos::FReal DSq = ToPt.SizeSquared();
		if (DSq > BestDSq)
		{
			BestDSq = DSq;
			AwayFromLineIdx = Idx;
			BasisVec = ToPt;
		}
	}

	ExtremesOut[2] = AwayFromLineIdx;

	Chaos::FVec3 PlaneNormal = BasisVec ^ LineDir;
	if (!PlaneNormal.Normalize())
	{
		return 2;
	}
	

	FChaosPlane Plane(LineStart, PlaneNormal);
	Chaos::FReal BestPlaneDist = -1;
	int32 AwayFromPlaneIdx = 0;
	for (int32 Idx = 0; Idx < NumPts; Idx++)
	{
		Chaos::FReal Dist = FMath::Abs(Plane.SignedDistance(InPts[Idx]));
		if (Dist > BestPlaneDist)
		{
			BestPlaneDist = Dist;
			AwayFromPlaneIdx = Idx;
		}
	}

	ExtremesOut[3] = AwayFromPlaneIdx;
	if (BestPlaneDist < KINDA_SMALL_NUMBER)
	{
		return 3;
	}

	return 4;
}

// filter points s.t. they are spaced at least more than SimplificationDistanceThreshold apart (after starting with the 4 'extreme' points to ensure we cover a volume)
void FilterHullPoints(const TArray<Chaos::FVec3>& InPts, TArray<Chaos::FVec3>& OutPts, double SimplificationDistanceThreshold)
{
	if (SimplificationDistanceThreshold > 0)
	{
		OutPts.Reset();

		int32 NumPts = InPts.Num();
		TArray<Chaos::FReal> DistSq;
		DistSq.SetNumUninitialized(NumPts);
		TArray<int32> PointOrder;
		PointOrder.SetNumUninitialized(NumPts);
		TPointHashGrid3<int32, Chaos::FReal> Spatial((Chaos::FReal)SimplificationDistanceThreshold, INDEX_NONE);
		Chaos::FAABB3 Bounds;
		for (int32 VIdx = 0; VIdx < NumPts; VIdx++)
		{
			Bounds.GrowToInclude(InPts[VIdx]);
			PointOrder[VIdx] = VIdx;
		}

		// Rank points by squared distance from center
		Chaos::FVec3 Center = Bounds.Center();
		for (int i = 0; i < NumPts; i++)
		{
			DistSq[i] = (InPts[i] - Center).SizeSquared();
		}

		// Start by picking the 'extreme' points to ensure we cover the volume reasonably (otherwise it's too easy to end up with a degenerate hull pieces)
		int32 ExtremePoints[4];
		int32 NumExtremes = PickExtremePoints(InPts, ExtremePoints);
		for (int32 ExtremeIdx = 0; ExtremeIdx < NumExtremes; ExtremeIdx++)
		{
			int32 ExtremePtIdx = ExtremePoints[ExtremeIdx];
			Chaos::FVec3 ExtremePt = InPts[ExtremePtIdx];
			OutPts.Add(ExtremePt);
			Spatial.InsertPointUnsafe(ExtremePtIdx, ExtremePt);
			DistSq[ExtremePtIdx] = -1; // remove these points from the distance ranking
		}

		// Sort in descending order
		Sort(&PointOrder[0], NumPts, [&DistSq](int32 i, int32 j)
			{
				return DistSq[i] > DistSq[j];
			});

		// Filter to include only w/ no other too-close points, prioritizing the farthest points
		for (int32 OrdIdx = 0; OrdIdx < NumPts; OrdIdx++)
		{
			int32 PtIdx = PointOrder[OrdIdx];
			if (DistSq[PtIdx] < 0)
			{
				break; // we've reached the extreme points that were already selected
			}
			Chaos::FVec3 Pt = InPts[PtIdx];
			TPair<int32, Chaos::FReal> NearestIdx = Spatial.FindNearestInRadius(Pt, (Chaos::FReal)SimplificationDistanceThreshold,
				[&InPts, &Pt](int32 Idx)
				{
					return (Pt - InPts[Idx]).SizeSquared();
				});
			if (NearestIdx.Key == INDEX_NONE)
			{
				Spatial.InsertPointUnsafe(PtIdx, Pt);
				OutPts.Add(Pt);
			}
		}
	}
	else
	{
		// No filtering requested -- in practice, we don't call this function in these cases below
		OutPts = InPts;
	}
}

void FilterHullPoints(TArray<Chaos::FVec3>& InOutPts, double SimplificationDistanceThreshold)
{
	if (SimplificationDistanceThreshold > 0)
	{
		TArray<Chaos::FVec3> FilteredPts;
		FilterHullPoints(InOutPts, FilteredPts, SimplificationDistanceThreshold);
		InOutPts = MoveTemp(FilteredPts);
	}
}

Chaos::FConvex MakeHull(const TArray<Chaos::FVec3>& Pts, double SimplificationDistanceThreshold)
{
	if (SimplificationDistanceThreshold > 0)
	{
		TArray<Chaos::FVec3> FilteredPts;
		FilterHullPoints(Pts, FilteredPts, SimplificationDistanceThreshold);
		
		return Chaos::FConvex(FilteredPts, KINDA_SMALL_NUMBER);
	}
	else
	{
		return Chaos::FConvex(Pts, KINDA_SMALL_NUMBER);
	}
}

/// Cut hull with plane, generating the point set of a new hull
/// @return false if plane did not cut any points on the hull
bool CutHull(const Chaos::FConvex& HullIn, FChaosPlane Plane, bool KeepSide, TArray<Chaos::FVec3>& HullPtsOut)
{
	const TArray<Chaos::FVec3>& Vertices = HullIn.GetVertices();
	const Chaos::FConvexStructureData& HullData = HullIn.GetStructureData();
	bool bHasOutside = false;
	for (int VertIdx = 0; VertIdx < Vertices.Num(); VertIdx++)
	{
		const Chaos::FVec3& V = Vertices[VertIdx];
		if ((Plane.SignedDistance(V) < 0) == KeepSide)
		{
			HullPtsOut.Add(V);
		}
		else
		{
			bHasOutside = true;
		}
	}

	if (!bHasOutside)
	{
		return false;
	}

	int32 NumPlanes = HullIn.NumPlanes();
	for (int PlaneIdx = 0; PlaneIdx < NumPlanes; PlaneIdx++)
	{
		int32 NumPlaneVerts = HullData.NumPlaneVertices(PlaneIdx);
		for (int32 PlaneVertexIdx = 0; PlaneVertexIdx < NumPlaneVerts; PlaneVertexIdx++)
		{
			int32 NextVertIdx = (PlaneVertexIdx + 1) % NumPlaneVerts;
			const Chaos::FVec3& V0 = Vertices[HullData.GetPlaneVertex(PlaneIdx, PlaneVertexIdx)];
			const Chaos::FVec3& V1 = Vertices[HullData.GetPlaneVertex(PlaneIdx, NextVertIdx)];
			if ((Plane.SignedDistance(V0) < 0) != (Plane.SignedDistance(V1) < 0))
			{
				Chaos::Pair<Chaos::FVec3, bool> Res = Plane.FindClosestIntersection(V0, V1, 0);
				if (Res.Second)
				{
					HullPtsOut.Add(Res.First);
				}
			}
		}
	}

	return true;
}


/// Cut hull with plane, keeping both sides and generating the point set of both new hulls
/// @return false if plane did not cut any points on the hull
bool SplitHull(const Chaos::FConvex& HullIn, FChaosPlane Plane, bool KeepSide, TArray<Chaos::FVec3>& InsidePtsOut, TArray<Chaos::FVec3>& OutsidePtsOut)
{
	const TArray<Chaos::FVec3>& Vertices = HullIn.GetVertices();
	const Chaos::FConvexStructureData& HullData = HullIn.GetStructureData();
	bool bHasOutside = false;
	for (int VertIdx = 0; VertIdx < Vertices.Num(); VertIdx++)
	{
		const Chaos::FVec3& V = Vertices[VertIdx];
		if ((Plane.SignedDistance(V) < 0) == KeepSide)
		{
			InsidePtsOut.Add(V);
		}
		else
		{
			OutsidePtsOut.Add(V);
			bHasOutside = true;
		}
	}

	if (!bHasOutside)
	{
		return false;
	}

	int32 NumPlanes = HullIn.NumPlanes();
	for (int PlaneIdx = 0; PlaneIdx < NumPlanes; PlaneIdx++)
	{
		int32 NumPlaneVerts = HullData.NumPlaneVertices(PlaneIdx);
		for (int32 PlaneVertexIdx = 0; PlaneVertexIdx < NumPlaneVerts; PlaneVertexIdx++)
		{
			int32 NextVertIdx = (PlaneVertexIdx + 1) % NumPlaneVerts;
			const Chaos::FVec3& V0 = Vertices[HullData.GetPlaneVertex(PlaneIdx, PlaneVertexIdx)];
			const Chaos::FVec3& V1 = Vertices[HullData.GetPlaneVertex(PlaneIdx, NextVertIdx)];
			if ((Plane.SignedDistance(V0) < 0) != (Plane.SignedDistance(V1) < 0))
			{
				Chaos::Pair<Chaos::FVec3, bool> Res = Plane.FindClosestIntersection(V0, V1, 0);
				if (Res.Second)
				{
					InsidePtsOut.Add(Res.First);
					OutsidePtsOut.Add(Res.First);
				}
			}
		}
	}

	return true;
}


/// Assumptions: 
///		Convexes is initialized to one convex hull for each leaf geometry in a SHARED coordinate space
///		TransformToConvexIndices is initialized to point to the existing convex hulls
///		Parents, GeoProximity, and GeometryToTransformIndex are all initialized from the geometry collection
void CreateNonoverlappingConvexHulls(
	TArray<TUniquePtr<Chaos::FConvex>>& Convexes,
	TArray<TSet<int32>>& TransformToConvexIndices,
	const TManagedArray<int32>& SimulationType,
	int32 LeafType,
	int32 SkipType,
	const TManagedArray<int32>& Parents,
	const TManagedArray<TSet<int32>>* GeoProximity,
	const TManagedArray<int32>& GeometryToTransformIndex,
	double FracAllowRemove,
	double SimplificationDistanceThreshold
)
{
	int32 NumBones = TransformToConvexIndices.Num();
	check(Parents.Num() == NumBones);

	auto SkipBone = [&SimulationType, SkipType](int32 Bone) -> bool
	{
		return SimulationType[Bone] == SkipType;
	};

	auto OnlyConvex = [&TransformToConvexIndices](int32 Bone) -> int32
	{
		ensure(TransformToConvexIndices[Bone].Num() <= 1);
		for (int32 ConvexIndex : TransformToConvexIndices[Bone])
		{
			return ConvexIndex;
		}
		return INDEX_NONE;
	};

	TArray<TSet<int32>> LeafProximity;
	LeafProximity.SetNum(NumBones);
	if (GeoProximity)
	{
		for (int32 GeomIdx = 0; GeomIdx < GeoProximity->Num(); GeomIdx++)
		{
			int32 TransformIdx = GeometryToTransformIndex[GeomIdx];
			for (int32 NbrGeomIdx : (*GeoProximity)[GeomIdx])
			{
				LeafProximity[TransformIdx].Add(GeometryToTransformIndex[NbrGeomIdx]);
			}
		}
	}
	else
	{
		// TODO: fill out LeafProximity by doing IsColliding calls (may have to construct Children array before this)
	}

	auto IsColliding = [&Convexes](int32 ConvexA, int32 ConvexB)
	{
		if (ConvexA == -1 || ConvexB == -1 || !ensure(Convexes[ConvexA]->NumVertices() > 0 && Convexes[ConvexB]->NumVertices() > 0))
		{
			// at least one of the convex hulls was empty, so cannot be colliding
			// TODO: eventually remove this ensure; and add more handling for empty hulls
			return false;
		}
		const Chaos::TRigidTransform<Chaos::FReal, 3> IdentityTransform = Chaos::TRigidTransform<Chaos::FReal, 3>::Identity;
		return GJKIntersection(*Convexes[ConvexA], *Convexes[ConvexB], IdentityTransform);
	};

	auto GetConvexSpan = [](const Chaos::FConvex& Convex, const Chaos::FVec3& Center, const Chaos::FVec3& Normal) -> Chaos::FVec2
	{
		int32 NumVertices = Convex.NumVertices();
		if (!ensure(NumVertices > 0))
		{
			return Chaos::FVec2(0, 0);
		}
		Chaos::FReal AlongFirst = Chaos::FVec3::DotProduct(Convex.GetVertex(0) - Center, Normal);
		Chaos::FVec2 Range(AlongFirst, AlongFirst);
		for (int Idx = 1; Idx < NumVertices; Idx++)
		{
			const float Along = static_cast<float>(Chaos::FVec3::DotProduct(Convex.GetVertex(Idx) - Center, Normal));
			if (Along < Range.X)
			{
				Range.X = Along;
			}
			else if (Along > Range.Y)
			{
				Range.Y = Along;
			}
		}
		return Range;
	};

	// Score separating plane direction based on how well it separates (lower is better)
	//  and also compute new center for plane + normal. Normal is either the input normal or flipped.
	auto ScoreCutPlane = [&GetConvexSpan](
		const Chaos::FConvex& A, const Chaos::FConvex& B,
		const FChaosPlane& Plane, bool bOneSidedCut,
		Chaos::FVec3& OutCenter, Chaos::FVec3& OutNormal) -> Chaos::FReal
	{
		bool bRangeAValid = false, bRangeBValid = false;
		Chaos::FVec2 RangeA = GetConvexSpan(A, Plane.X(), Plane.Normal());
		Chaos::FVec2 RangeB = GetConvexSpan(B, Plane.X(), Plane.Normal());
		Chaos::FVec2 Union(FMath::Min(RangeA.X, RangeB.X), FMath::Max(RangeA.Y, RangeB.Y));
		// no intersection -- cut plane is separating, this is ideal!
		if (RangeA.X > RangeB.Y || RangeA.Y < RangeB.X)
		{
			if (RangeA.X > RangeB.Y)
			{
				OutCenter = Plane.X() + Plane.Normal() * ((RangeA.X + RangeB.Y) * (Chaos::FReal).5);
				OutNormal = -Plane.Normal();
			}
			else
			{
				OutCenter = Plane.X() + Plane.Normal() * ((RangeA.Y + RangeB.X) * (Chaos::FReal).5);
				OutNormal = Plane.Normal();
			}
			return 0;
		}
		// there was an intersection; find the actual mid plane-center and score it
		Chaos::FVec2 Intersection(FMath::Max(RangeA.X, RangeB.X), FMath::Min(RangeA.Y, RangeB.Y));
		Chaos::FReal IntersectionMid = (Intersection.X + Intersection.Y) * (Chaos::FReal).5;

		// Decide which side of the plane is kept/removed
		Chaos::FVec2 BiggerRange = RangeA;
		Chaos::FReal Sign = 1;
		if (RangeA.Y - RangeA.X < RangeB.Y - RangeB.X)
		{
			BiggerRange = RangeB;
			Sign = -1;
		}
		if (IntersectionMid - BiggerRange.X < BiggerRange.Y - IntersectionMid)
		{
			Sign *= -1;
		}
		OutNormal = Sign * Plane.Normal();

		Chaos::FReal IntersectionCut = IntersectionMid;
		if (bOneSidedCut) // if cut is one-sided, move the plane to the far end of Convex B (which it should not cut)
		{
			// which end depends on which way the output cut plane is oriented
			if (Sign > 0)
			{
				IntersectionCut = RangeB.X;
			}
			else
			{
				IntersectionCut = RangeB.Y;
			}
		}
		OutCenter = Plane.X() + Plane.Normal() * IntersectionCut;

		// Simple score favors small intersection span relative to union span
		// TODO: consider other metrics; e.g. something more directly ~ percent cut away
		return (Intersection.Y - Intersection.X) / (Union.Y - Union.X);
	};

	// Search cut plane options for the most promising one -- 
	// Usually GJK gives a good cut plane, but it can fail badly so we also test a simple 'difference between centers' plane
	// TODO: consider adding more plane options to the search
	auto FindCutPlane = [&ScoreCutPlane](const Chaos::FConvex& A, const Chaos::FConvex& B,
		Chaos::FVec3 CloseA, Chaos::FVec3 CloseB, Chaos::FVec3 Normal,
		bool bOneSidedCut,
		Chaos::FVec3& OutCenter, Chaos::FVec3& OutNormal) -> bool
	{
		OutCenter = (CloseA + CloseB) * .5;
		OutNormal = Normal;
		Chaos::FVec3 GJKCenter;
		Chaos::FReal BestScore = ScoreCutPlane(A, B, FChaosPlane(OutCenter, OutNormal), bOneSidedCut, OutCenter, OutNormal);
		Chaos::FVec3 MassSepNormal = (B.GetCenterOfMass() - A.GetCenterOfMass());
		if (MassSepNormal.Normalize() && BestScore > 0)
		{
			Chaos::FVec3 MassSepCenter = (A.GetCenterOfMass() + B.GetCenterOfMass()) * .5;
			Chaos::FReal ScoreB = ScoreCutPlane(A, B,
				FChaosPlane(MassSepCenter, MassSepNormal), bOneSidedCut, MassSepCenter, MassSepNormal);
			if (ScoreB < BestScore)
			{
				BestScore = ScoreB;
				OutCenter = MassSepCenter;
				OutNormal = MassSepNormal;
			}
		}
		if (BestScore == 0)
		{
			return false;
		}
		return true;
	};

	auto FixCollisionWithCut = [&Convexes, &FindCutPlane, &SimplificationDistanceThreshold](int32 ConvexA, int32 ConvexB)
	{
		if (!ensure(Convexes[ConvexA]->NumVertices() > 0 && Convexes[ConvexB]->NumVertices() > 0))
		{
			// at least one of the convex hulls was empty, so cannot be colliding
			// TODO: eventually remove this ensure; and add more handling for empty hulls
			return false;
		}
		Chaos::FReal Depth;
		Chaos::FVec3 CloseA, CloseB, Normal;
		int32 OutIdxA, OutIdxB;
		const Chaos::TRigidTransform<Chaos::FReal, 3> IdentityTransform = Chaos::TRigidTransform<Chaos::FReal, 3>::Identity;
		bool bCollide = Chaos::GJKPenetration(*Convexes[ConvexA], *Convexes[ConvexB], IdentityTransform, Depth, CloseA, CloseB, Normal, OutIdxA, OutIdxB);
		if (bCollide)
		{
			Chaos::FVec3 IdealCenter, IdealNormal;
			if (!FindCutPlane(*Convexes[ConvexA], *Convexes[ConvexB], CloseA, CloseB, Normal, false, IdealCenter, IdealNormal))
			{
				return false;
			}
			FChaosPlane CutPlane(IdealCenter, IdealNormal);
			TArray<Chaos::FVec3> CutHullPts;
			if (CutHull(*Convexes[ConvexA], CutPlane, true, CutHullPts))
			{
				*Convexes[ConvexA] = MakeHull(CutHullPts, SimplificationDistanceThreshold);
			}
			CutHullPts.Reset();
			if (CutHull(*Convexes[ConvexB], CutPlane, false, CutHullPts))
			{
				*Convexes[ConvexB] = MakeHull(CutHullPts, SimplificationDistanceThreshold);
			}
		}
		return bCollide;
	};

	// Initialize Children and Depths of tree
	// Fix collisions between input hulls using the input proximity relationships

	int32 MaxDepth = 0;
	TArray<int32> Depths;
	TArray<TArray<int32>> Children;
	Children.SetNum(NumBones);
	Depths.SetNumZeroed(NumBones);
	for (int HullIdx = 0; HullIdx < NumBones; HullIdx++)
	{
		if (Parents[HullIdx] != INDEX_NONE)
		{
			if (SimulationType[Parents[HullIdx]] != LeafType)
			{
				Children[Parents[HullIdx]].Add(HullIdx);
			}
			else
			{
				Depths[HullIdx] = -1; // child-of-leaf == embedded geometry, just ignore it
				continue;
			}
		}
		int32 Depth = 0, WalkParent = HullIdx;
		while (Parents[WalkParent] != INDEX_NONE)
		{
			Depth++;
			WalkParent = Parents[WalkParent];
		}
		Depths[HullIdx] = Depth;
		MaxDepth = FMath::Max(Depth, MaxDepth);

		if (TransformToConvexIndices[HullIdx].Num() > 0)
		{
			const TSet<int32>& Neighbors = LeafProximity[HullIdx];
			for (int32 NbrIdx : Neighbors)
			{
				if (NbrIdx < HullIdx && TransformToConvexIndices[NbrIdx].Num() > 0)
				{
					FixCollisionWithCut(OnlyConvex(HullIdx), OnlyConvex(NbrIdx));
				}
			}
		}
	}

	auto AddLeaves = [&Children, &SimulationType, LeafType](int32 Bone, TArray<int32>& Leaves)
	{
		TArray<int32> ToExpand;
		ToExpand.Add(Bone);
		while (ToExpand.Num() > 0)
		{
			int32 ToProcess = ToExpand.Pop(false);
			if (SimulationType[ToProcess] == LeafType)
			{
				Leaves.Add(ToProcess);
			}
			else if (Children[ToProcess].Num() > 0)
			{
				ToExpand.Append(Children[ToProcess]);
			}
		}
	};

	auto FixLeafCollisions = [&AddLeaves, &FixCollisionWithCut, &OnlyConvex](int32 BoneA, int32 BoneB)
	{
		TArray<int32> LeavesA, LeavesB;
		AddLeaves(BoneA, LeavesA);
		AddLeaves(BoneB, LeavesB);
		bool bAnyCollides = false;
		for (int32 LeafA : LeavesA)
		{
			for (int32 LeafB : LeavesB)
			{
				bool bCollides = FixCollisionWithCut(OnlyConvex(LeafA), OnlyConvex(LeafB));
				bAnyCollides |= bCollides;
			}
		}
		return bAnyCollides;
	};

	// Compute initial hulls at all levels, and use these to fill out the full proximity links
	// Fix collisions between any two leaf hulls

	TArray<TSet<int32>> ClusterProximity;
	ClusterProximity.SetNum(NumBones);
	int ProcessDepth = MaxDepth;
	while (--ProcessDepth >= 0)
	{
		TSet<int32> ToProcess;
		for (int Bone = 0; Bone < Parents.Num(); Bone++)
		{
			if (Depths[Bone] == ProcessDepth)
			{
				if (TransformToConvexIndices[Bone].Num() == 0)
				{
					TArray<Chaos::FVec3> JoinedHullPts;
					for (int32 Child : Children[Bone])
					{
						for (int32 ConvexIdx : TransformToConvexIndices[Child])
						{
							JoinedHullPts.Append(Convexes[ConvexIdx]->GetVertices());
						}
					}
					if (JoinedHullPts.Num() > 0)
					{
						FilterHullPoints(JoinedHullPts, SimplificationDistanceThreshold);
						int32 ConvexIdx = Convexes.Add(MakeUnique<Chaos::FConvex>(JoinedHullPts, KINDA_SMALL_NUMBER));
						TransformToConvexIndices[Bone].Add(ConvexIdx);
						ToProcess.Add(Bone);
					}
				}
				else
				{
					ToProcess.Add(Bone);
				}
			}
		}

		ensure(ToProcess.Num() > 0);

		// We don't have proximity at the cluster level, so reconstruct from n^2 collision tests for now
		for (int32 BoneA : ToProcess)
		{
			for (int32 BoneB : ToProcess)
			{
				if (BoneB < BoneA)
				{
					if (IsColliding(OnlyConvex(BoneA), OnlyConvex(BoneB)))
					{
						bool bLeavesCollide = FixLeafCollisions(BoneA, BoneB);
						int32 Bones[2]{ BoneA, BoneB };
						for (int32 BoneIdx = 0; BoneIdx < 2; BoneIdx++)
						{
							int32 ParentBone = Bones[BoneIdx];
							int32 OtherBone = Bones[1 - BoneIdx];
							// if leaves changed flag that all hulls using those leaves must also change
							TArray<int32> TraverseBones;
							TraverseBones.Append(Children[ParentBone]);
							while (TraverseBones.Num() > 0)
							{
								int32 ToProc = TraverseBones.Pop(false);
								if (IsColliding(OnlyConvex(OtherBone), OnlyConvex(ToProc)))
								{
									ClusterProximity[OtherBone].Add(ToProc);
									ClusterProximity[ToProc].Add(OtherBone);
									TraverseBones.Append(Children[ToProc]);
								}
							}
						}

						ClusterProximity[BoneA].Add(BoneB);
						ClusterProximity[BoneB].Add(BoneA);
					}
				}
			}
		}
	}

	// Now that leaves don't intersect, re-compute all non-leaf hulls on the clipped leaves
	// and record volumes for each hull

	TArray<double> NonLeafVolumes; // Original volumes of non-leaf hulls (to compare against progressively cut-down volume as intersections are removed)
	NonLeafVolumes.SetNumZeroed(Convexes.Num());
	ProcessDepth = MaxDepth;
	while (--ProcessDepth > 0)
	{
		TSet<int32> ToProcess;
		for (int32 Bone = 0; Bone < NumBones; Bone++)
		{
			if (Depths[Bone] == ProcessDepth && Children[Bone].Num() > 0 && !SkipBone(Bone))
			{
				TArray<Chaos::FVec3> JoinedHullPts;
				for (int32 Child : Children[Bone])
				{
					for (int32 ConvexIdx : TransformToConvexIndices[Child])
					{
						JoinedHullPts.Append(Convexes[ConvexIdx]->GetVertices());
					}
				}
				int32 ConvexIdx = OnlyConvex(Bone);
				if (ConvexIdx)
				{
					*Convexes[ConvexIdx] = MakeHull(JoinedHullPts, SimplificationDistanceThreshold);
					NonLeafVolumes[ConvexIdx] = Convexes[ConvexIdx]->GetVolume();
				}
			}
		}
	}

	auto CutIfOk = [&Convexes, &OnlyConvex, &NonLeafVolumes, &FindCutPlane, &FracAllowRemove, &SimplificationDistanceThreshold](bool bOneSidedCut, int32 ConvexA, int32 ConvexB) -> bool
	{
		Chaos::FReal Depth;
		Chaos::FVec3 CloseA, CloseB, Normal;
		int32 OutIdxA, OutIdxB;
		const Chaos::TRigidTransform<Chaos::FReal, 3> IdentityTransform = Chaos::TRigidTransform<Chaos::FReal, 3>::Identity;
		bool bCollide = Chaos::GJKPenetration(*Convexes[ConvexA], *Convexes[ConvexB], IdentityTransform, Depth, CloseA, CloseB, Normal, OutIdxA, OutIdxB);
		if (bCollide)
		{
			Chaos::FVec3 IdealCenter, IdealNormal;
			FindCutPlane(*Convexes[ConvexA], *Convexes[ConvexB], CloseA, CloseB, Normal, bOneSidedCut, IdealCenter, IdealNormal);
			FChaosPlane CutPlane(IdealCenter, IdealNormal);

			// Tentatively create the clipped hulls
			Chaos::FConvex CutHullA, CutHullB;
			bool bCreatedA = false, bCreatedB = false;
			TArray<Chaos::FVec3> CutHullPts;
			if (CutHull(*Convexes[ConvexA], CutPlane, true, CutHullPts))
			{
				if (CutHullPts.Num() < 4) // immediate reject zero-volume results
				{
					return false;
				}
				CutHullA = MakeHull(CutHullPts, SimplificationDistanceThreshold);
				bCreatedA = true;
			}
			if (!bOneSidedCut)
			{
				CutHullPts.Reset();
				if (CutHull(*Convexes[ConvexB], CutPlane, false, CutHullPts))
				{
					CutHullB = MakeHull(CutHullPts, SimplificationDistanceThreshold);
					bCreatedB = true;
				}
			}

			// Test if the clipped hulls have become too small vs the original volumes
			if (ensure(ConvexA < NonLeafVolumes.Num()) && NonLeafVolumes[ConvexA] > 0 && bCreatedA && CutHullA.GetVolume() / NonLeafVolumes[ConvexA] < 1 - FracAllowRemove)
			{
				return false;
			}
			if (!bOneSidedCut)
			{
				if (ensure(ConvexB < NonLeafVolumes.Num()) && NonLeafVolumes[ConvexB] > 0 && bCreatedB && CutHullB.GetVolume() / NonLeafVolumes[ConvexB] < 1 - FracAllowRemove)
				{
					return false;
				}
			}

			// If the clipped hulls were large enough, go ahead and set them as the new hulls
			if (bCreatedA)
			{
				*Convexes[ConvexA] = MoveTemp(CutHullA);
			}
			if (bCreatedB)
			{
				*Convexes[ConvexB] = MoveTemp(CutHullB);
			}

			return true;
		}
		else
		{
			return true; // no cut needed, so was ok
		}
	};

	// re-process all non-leaf bones
	ProcessDepth = MaxDepth;
	while (--ProcessDepth > 0)
	{
		TSet<int32> ToProcess;
		for (int32 Bone = 0; Bone < NumBones; Bone++)
		{
			if (Depths[Bone] == ProcessDepth && Children[Bone].Num() > 0)
			{
				ToProcess.Add(Bone);
			}
		}

		TSet<int32> WasNotOk;

		for (int32 Bone : ToProcess)
		{
			if (WasNotOk.Contains(Bone) || TransformToConvexIndices[Bone].Num() == 0)
			{
				continue;
			}
			for (int32 Nbr : ClusterProximity[Bone])
			{
				if (WasNotOk.Contains(Nbr) || TransformToConvexIndices[Nbr].Num() == 0)
				{
					continue;
				}
				bool bAllOk = true;
				for (int32 ConvexBone : TransformToConvexIndices[Bone])
				{
					for (int32 ConvexNbr : TransformToConvexIndices[Nbr])
					{
						bool bOneSidedCut = Depths[Bone] != Depths[Nbr] || Children[Nbr].Num() == 0;
						// if the neighbor is less deep and not a leaf, skip processing this to favor processing the neighbor instead
						if (Depths[Bone] > Depths[Nbr] && Children[Nbr].Num() > 0)
						{
							continue;
						}

						bool bWasOk = CutIfOk(bOneSidedCut, ConvexBone, ConvexNbr);

						// cut would have removed too much; just fall back to using the hulls of children
						// TODO: attempt splitting hulls before fully falling back to this
						if (!bWasOk)
						{
							bAllOk = false;
							auto ResetBoneHulls = [&WasNotOk, &TransformToConvexIndices](int32 ToReset)
							{
								WasNotOk.Add(ToReset);
								TransformToConvexIndices[ToReset].Reset();
							};
							ResetBoneHulls(Bone);
							if (!bOneSidedCut)
							{
								ResetBoneHulls(Nbr);
							}
							break;
						}
					}
					if (!bAllOk)
					{
						break;
					}
				}
			}
		}
	}

	return;
}


/// Helper to get convex hulls from a geometry collection in the format required by CreateNonoverlappingConvexHulls
void HullsFromGeometry(
	const FGeometryCollection& Geometry,
	TArray<FTransform>& GlobalTransformArray,
	TArray<TUniquePtr<Chaos::FConvex>>& Convexes,
	TArray<TSet<int32>>& TransformToConvexIndices,
	const TManagedArray<int32>& SimulationType,
	int32 RigidType,
	double SimplificationDistanceThreshold
)
{
	TArray<FVector> GlobalVertices;
	
	GlobalVertices.SetNum(Geometry.Vertex.Num());
	for (int32 Idx = 0; Idx < GlobalVertices.Num(); Idx++)
	{
		GlobalVertices[Idx] = GlobalTransformArray[Geometry.BoneMap[Idx]].TransformPosition(Geometry.Vertex[Idx]);
	}

	int32 NumBones = Geometry.TransformToGeometryIndex.Num();
	TransformToConvexIndices.SetNum(NumBones);
	for (int32 Idx = 0; Idx < NumBones; Idx++)
	{
		if (SimulationType[Idx] != RigidType)
		{
			continue;
		}
		int32 GeomIdx = Geometry.TransformToGeometryIndex[Idx];
		if (GeomIdx != INDEX_NONE)
		{
			int32 VStart = Geometry.VertexStart[GeomIdx];
			int32 VCount = Geometry.VertexCount[GeomIdx];
			int32 VEnd = VStart + VCount;
			TArray<Chaos::FVec3> HullPts;
			HullPts.Reserve(VCount);
			for (int32 VIdx = VStart; VIdx < VEnd; VIdx++)
			{
				HullPts.Add(GlobalVertices[VIdx]);
			}
			ensure(HullPts.Num() > 0);
			FilterHullPoints(HullPts, SimplificationDistanceThreshold);
			int32 ConvexIdx = Convexes.Add(MakeUnique<Chaos::FConvex>(HullPts, KINDA_SMALL_NUMBER));
			if (Convexes[ConvexIdx]->NumVertices() == 0 && HullPts.Num() > 0)
			{
				// if we've failed to make a convex hull, add a tiny bounding box just to ensure every geometry has a hull
				Chaos::FAABB3 AABB = Convexes[ConvexIdx]->BoundingBox();
				AABB.Thicken((Chaos::FReal).001);
				Chaos::FVec3 Min = AABB.Min();
				Chaos::FVec3 Max = AABB.Max();
				HullPts.Add(Min);
				HullPts.Add(Max);
				HullPts.Add(Chaos::FVec3(Min.X, Min.Y, Max.Z));
				HullPts.Add(Chaos::FVec3(Min.X, Max.Y, Max.Z));
				HullPts.Add(Chaos::FVec3(Max.X, Min.Y, Max.Z));
				HullPts.Add(Chaos::FVec3(Max.X, Max.Y, Min.Z));
				HullPts.Add(Chaos::FVec3(Max.X, Min.Y, Min.Z));
				HullPts.Add(Chaos::FVec3(Min.X, Max.Y, Min.Z));
				// note: Do not use SimplificationDistanceThreshold for this fixed tiny hull
				*Convexes[ConvexIdx] = Chaos::FConvex(HullPts, KINDA_SMALL_NUMBER);
			}
			ensure(Convexes[ConvexIdx]->NumVertices() > 0);
			TransformToConvexIndices[Idx].Add(ConvexIdx);
		}
	}
}

void TransformHullsToLocal(
	TArray<FTransform>& GlobalTransformArray,
	TArray<TUniquePtr<Chaos::FConvex>>& Convexes,
	TArray<TSet<int32>>& TransformToConvexIndices
)
{
	for (int32 Bone = 0; Bone < TransformToConvexIndices.Num(); Bone++)
	{
		FTransform& Transform = GlobalTransformArray[Bone];
		TArray<Chaos::FVec3> HullPts;
		for (int32 ConvexIdx : TransformToConvexIndices[Bone])
		{
			HullPts.Reset();
			for (const Chaos::FVec3& P : Convexes[ConvexIdx]->GetVertices())
			{
				HullPts.Add(Transform.InverseTransformPosition(P));
			}
			// Do not simplify hulls when we're just trying to transform them
			*Convexes[ConvexIdx] = Chaos::FConvex(HullPts, KINDA_SMALL_NUMBER);
		}
	}
}


}


FGeometryCollectionConvexUtility::FGeometryCollectionConvexData FGeometryCollectionConvexUtility::CreateNonOverlappingConvexHullData(FGeometryCollection* GeometryCollection, double FracAllowRemove, double SimplificationDistanceThreshold)
{
	check(GeometryCollection);

	bool bHasProximity = GeometryCollection->HasAttribute("Proximity", FGeometryCollection::GeometryGroup);
	const TManagedArray<TSet<int32>>* GCProximity = GeometryCollection->FindAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup);
	TArray<TUniquePtr<Chaos::FConvex>> Convexes;
	TArray<TSet<int32>> TransformToConvexIndexArr;
	TArray<FTransform> GlobalTransformArray;
	GeometryCollectionAlgo::GlobalMatrices(GeometryCollection->Transform, GeometryCollection->Parent, GlobalTransformArray);
	HullsFromGeometry(*GeometryCollection, GlobalTransformArray, Convexes, TransformToConvexIndexArr, GeometryCollection->SimulationType, FGeometryCollection::ESimulationTypes::FST_Rigid, SimplificationDistanceThreshold);

	CreateNonoverlappingConvexHulls(Convexes, TransformToConvexIndexArr, GeometryCollection->SimulationType, FGeometryCollection::ESimulationTypes::FST_Rigid, FGeometryCollection::ESimulationTypes::FST_None, GeometryCollection->Parent, GCProximity, GeometryCollection->TransformIndex, FracAllowRemove, SimplificationDistanceThreshold);

	TransformHullsToLocal(GlobalTransformArray, Convexes, TransformToConvexIndexArr);

	if (!GeometryCollection->HasGroup("Convex"))
	{
		GeometryCollection->AddGroup("Convex");
	}

	if (!GeometryCollection->HasAttribute("TransformToConvexIndices", FTransformCollection::TransformGroup))
	{
		FManagedArrayCollection::FConstructionParameters ConvexDependency("Convex");
		GeometryCollection->AddAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup, ConvexDependency);
	}

	if (!GeometryCollection->HasAttribute("ConvexHull", "Convex"))
	{
		GeometryCollection->AddAttribute<TUniquePtr<Chaos::FConvex>>("ConvexHull", "Convex");
	}

	TManagedArray<TSet<int32>>& TransformToConvexIndices = GeometryCollection->GetAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup);
	TManagedArray<TUniquePtr<Chaos::FConvex>>& ConvexHull = GeometryCollection->GetAttribute<TUniquePtr<Chaos::FConvex>>("ConvexHull", "Convex");
	TransformToConvexIndices = MoveTemp(TransformToConvexIndexArr);
	GeometryCollection->Resize(Convexes.Num(), "Convex");
	ConvexHull = MoveTemp(Convexes);

	return { TransformToConvexIndices, ConvexHull };
}



TUniquePtr<Chaos::FConvex> FGeometryCollectionConvexUtility::FindConvexHull(const FGeometryCollection* GeometryCollection, int32 GeometryIndex)
{
	check(GeometryCollection);

	int32 VertexCount = GeometryCollection->VertexCount[GeometryIndex];
	int32 VertexStart = GeometryCollection->VertexStart[GeometryIndex];

	TArray<Chaos::FVec3> Vertices;
	Vertices.SetNum(VertexCount);
	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		Vertices[VertexIndex] = GeometryCollection->Vertex[VertexStart+VertexIndex];
	}

	return MakeUnique<Chaos::FConvex>(Vertices, 0.0f);
}


void FGeometryCollectionConvexUtility::RemoveConvexHulls(FGeometryCollection* GeometryCollection, const TArray<int32>& SortedTransformDeletes)
{
	if (GeometryCollection->HasGroup("Convex") && GeometryCollection->HasAttribute("TransformToConvexIndices", FTransformCollection::TransformGroup))
	{
		TManagedArray<TSet<int32>>& TransformToConvexIndices = GeometryCollection->GetAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup);
		TArray<int32> ConvexIndices;
		for (int32 TransformIdx : SortedTransformDeletes)
		{
			if (TransformToConvexIndices[TransformIdx].Num() > 0)
			{
				for (int32 ConvexIdx : TransformToConvexIndices[TransformIdx])
				{
					ConvexIndices.Add(ConvexIdx);
				}
				TransformToConvexIndices[TransformIdx].Empty();
			}
		}

		if (ConvexIndices.Num())
		{
			ConvexIndices.Sort();
			GeometryCollection->RemoveElements("Convex", ConvexIndices);
		}
	}
}


void FGeometryCollectionConvexUtility::SetDefaults(FGeometryCollection* GeometryCollection, FName Group, uint32 StartSize, uint32 NumElements)
{
}