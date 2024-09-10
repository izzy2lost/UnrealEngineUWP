// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialAlgo/PCGAStar.h"

#include "Helpers/PCGHelpers.h"
#include "SpatialAlgo/PCGOctreeQueries.h"

namespace PCGSpatialAlgo::AStar
{
namespace Helpers
{
	bool CompareNodes(const FNode& Node1, const FNode& Node2)
	{
		return Node1.EstimatedGoalCost < Node2.EstimatedGoalCost;
	}

	void BuildFinalPath(const FSearchState& SearchState, const FNode& FinalNode, const bool bCopyOriginatingPoints, TArray<FPCGPoint>& OutPath)
	{
		check(&FinalNode);

		OutPath.Reset();

		const FNode* Node = &FinalNode;
		do // Build path from the goal backwards.
		{
			check(Node->PCGPoint);
			if (bCopyOriginatingPoints)
			{
				OutPath.Emplace(*Node->PCGPoint);
			}
			else
			{
				OutPath.Emplace(Node->PCGPoint->Transform, /*InDensity=*/1, PCGHelpers::ComputeSeedFromPosition(Node->PCGPoint->Transform.GetLocation()));
			}

			Node = Node->PreviousNodeIndex != INDEX_NONE ? &SearchState.NodeList[Node->PreviousNodeIndex] : nullptr;
		}
		while (Node);

		// Reverse to get it in the correct travel order.
		Algo::Reverse(OutPath);
	}
}

namespace Cost
{
	double CalculateCost_EuclideanDistance(const double PreviousNodeCost, const double DistanceToPreviousNodeSquared)
	{
		return PreviousNodeCost + FMath::Sqrt(DistanceToPreviousNodeSquared);
	}
}

/** Note: In order for the path to be optimal, the heuristic cost must always be less than or equal to the actual cost. */
namespace Heuristic
{
	double CalculateHeuristic_EuclideanDistance(const FVector& CurrentLocation, const FVector& GoalLocation)
	{
		return FVector::Dist(CurrentLocation, GoalLocation);
	}
}

/** Initialize the search state for AStar. Must be called before ExecuteSearchIteration. */
void Initialize(const UPCGPointData* const PointData, const FSearchSettings& Settings, FSearchState& OutSearchState)
{
	check(PointData && !PointData->IsEmpty());

	// Emplace the starting node on the open list. The costs will be 0 at the starting point.
	OutSearchState.NodeList.Reset();
	OutSearchState.NodeList.Emplace(&Settings.StartPoint, /*InParent=*/INDEX_NONE, /*InCost=*/0.0, /*HeuristicCost=*/0.0);
	OutSearchState.OpenIndexList.Reset();
	// Okay to add the first index this way, since it will be "heapified" by default with one element.
	OutSearchState.OpenIndexList.Add(0);
	OutSearchState.ClosedIndexList.Reset();
}

/** Runs a single iteration of the A* algorithm. Intended to be called multiple times, whereupon it will return true when the algorithm is finished. */
bool ExecuteSearchIteration(const FSearchSettings& SearchSettings, FSearchState& SearchState, TArray<FPCGPoint>& OutPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPathfindingElement::Algorithm::AStar);
	check(SearchState.OriginatingPointData);

	// The starting node should be guaranteed by the caller.
	if (!ensure(!SearchState.OpenIndexList.IsEmpty()))
	{
		return true;
	}

	TArray<FNode, TInlineAllocator<FSearchState::PreAllocNodeCount>>& NodeList = SearchState.NodeList;
	// Get the lowest cost point on the list, which has been binary heap sorted.
	int32 CurrentNodeIndex;
	SearchState.OpenIndexList.HeapPop(CurrentNodeIndex, [&SearchState](int32 Index1, int32 Index2) { return Helpers::CompareNodes(SearchState.NodeList[Index1], SearchState.NodeList[Index2]); }, EAllowShrinking::No);
	check(NodeList[CurrentNodeIndex].PCGPoint);

	// Arrived at the goal. Add the goal to the path and return it. TODO: Will need to be updated when cost functions consider more than just distance.
	if (FVector::DistSquared(NodeList[CurrentNodeIndex].PCGPoint->Transform.GetLocation(), SearchSettings.Goal) < SearchSettings.SearchDistance * SearchSettings.SearchDistance)
	{
		const FPCGPoint GoalPoint(FTransform(SearchSettings.Goal), /*InDensity=*/1, PCGHelpers::ComputeSeedFromPosition(SearchSettings.Goal));
		const FNode GoalNode(&GoalPoint, CurrentNodeIndex, NodeList[CurrentNodeIndex].EstimatedGoalCost, /*bInHeuristicCost=*/0);
		Helpers::BuildFinalPath(SearchState, GoalNode, SearchSettings.bCopyOriginatingPoints, OutPath);
		return true;
	}

	// Gather neighbors within the search radius.
	UPCGOctreeQueries::ForEachPointInsideSphere(
			SearchState.OriginatingPointData,
			NodeList[CurrentNodeIndex].PCGPoint->Transform.GetLocation(),
			SearchSettings.SearchDistance,
			[&NodeList, CurrentNodeIndex, &SearchSettings, &SearchState](const FPCGPointRef& PointRef, const double DistanceToPointSquared)
			{
				const bool bTrackedPoint = SearchState.PointToNodeIndexMap.Contains(PointRef.Point);
				// Node has already been ruled out.
				if (bTrackedPoint && SearchState.ClosedIndexList.Contains(SearchState.PointToNodeIndexMap[PointRef.Point]))
				{
					return;
				}

				const double TentativeNewLocalCost = Cost::CalculateCost_EuclideanDistance(NodeList[CurrentNodeIndex].LocalCost, DistanceToPointSquared);

				// Not tracking this point yet--add it to the node list and map it.
				if (!bTrackedPoint)
				{
					const double HeuristicCost = SearchSettings.HeuristicWeight * Heuristic::CalculateHeuristic_EuclideanDistance(PointRef.Point->Transform.GetLocation(), SearchSettings.Goal);
					NodeList.Emplace(PointRef.Point, CurrentNodeIndex, TentativeNewLocalCost, TentativeNewLocalCost + HeuristicCost);
					const int32 NewNodeIndex = NodeList.Num() - 1;
					SearchState.OpenIndexList.HeapPush(NewNodeIndex, [&SearchState](int32 Index1, int32 Index2) { return Helpers::CompareNodes(SearchState.NodeList[Index1], SearchState.NodeList[Index2]); });
					SearchState.PointToNodeIndexMap.Emplace(PointRef.Point, NewNodeIndex);

					return;
				}

				// Check if the path to this neighbor is a better path than its current one. If so, update accordingly.
				const int32 NeighborIndex = SearchState.PointToNodeIndexMap[PointRef.Point];
				FNode& Neighbor = SearchState.NodeList[NeighborIndex];
				if (TentativeNewLocalCost < Neighbor.LocalCost)
				{
					check(SearchState.OpenIndexList.Contains(NeighborIndex));

					const double HeuristicCost = SearchSettings.HeuristicWeight * Heuristic::CalculateHeuristic_EuclideanDistance(PointRef.Point->Transform.GetLocation(), SearchSettings.Goal);
					Neighbor.PreviousNodeIndex = CurrentNodeIndex;
					Neighbor.LocalCost = TentativeNewLocalCost;
					Neighbor.EstimatedGoalCost = TentativeNewLocalCost + HeuristicCost;
				}
			});

	// This node has been completely evaluated.
	SearchState.ClosedIndexList.Add(CurrentNodeIndex);

	// Final point to check, no path can be found.
	if (SearchState.OpenIndexList.IsEmpty())
	{
		OutPath.Reset();

		if (SearchSettings.bAcceptPartialPath)
		{
			auto DistanceSquaredToGoal = [&NodeList = SearchState.NodeList, &Goal = SearchSettings.Goal](const int32 NodeIndex)
			{
				return FVector::DistSquared(NodeList[NodeIndex].PCGPoint->Transform.GetLocation(), Goal);
			};

			if (const int32* MaxNodeIndex = Algo::MinElementBy(SearchState.ClosedIndexList, DistanceSquaredToGoal))
			{
				Helpers::BuildFinalPath(SearchState, SearchState.NodeList[*MaxNodeIndex], SearchSettings.bCopyOriginatingPoints, OutPath);
			}
		}

		return true;
	}

	return false;
}
} // namespace PCGSpatialAlgo::AStar
