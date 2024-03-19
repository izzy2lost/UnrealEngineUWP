// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/ChunkDependencyInfo.h"
#include "Algo/Unique.h"

DEFINE_LOG_CATEGORY_STATIC(LogChunkDependencyInfo, Log, All);


UChunkDependencyInfo::UChunkDependencyInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CachedHighestChunk = -1;
}

const FChunkDependencyTreeNode* UChunkDependencyInfo::GetOrBuildChunkDependencyGraph(int32 HighestChunk, bool bForceRebuild)
{
	if (HighestChunk > CachedHighestChunk)
	{
		return BuildChunkDependencyGraph(HighestChunk);
	}
	else if (bForceRebuild)
	{
		return BuildChunkDependencyGraph(CachedHighestChunk);
	}
	return &RootTreeNode;
}

const FChunkDependencyTreeNode* UChunkDependencyInfo::BuildChunkDependencyGraph(int32 HighestChunk)
{
	// Reset any current tree
	RootTreeNode.ChunkID = 0;
	RootTreeNode.ChildNodes.Reset(0);

	ChildToParentMap.Reset();
	CachedHighestChunk = HighestChunk;

	// Ensure the DependencyArray is OK to work with.
	for (int32 DepIndex = DependencyArray.Num() - 1; DepIndex >= 0; DepIndex --)
	{
		const FChunkDependency& Dep = DependencyArray[DepIndex];
		if (Dep.ChunkID > HighestChunk)
		{
			HighestChunk = Dep.ChunkID;
		}
		if (Dep.ParentChunkID > HighestChunk)
		{
			HighestChunk = Dep.ParentChunkID;
		}
		if (Dep.ChunkID == Dep.ParentChunkID)
		{
			// Remove cycles
			DependencyArray.RemoveAtSwap(DepIndex);
		}
	}
	// Add missing links (assumes they parent to chunk zero)
	for (int32 i = 1; i <= HighestChunk; ++i)
	{
		if (!DependencyArray.FindByPredicate([=](const FChunkDependency& RHS){ return i == RHS.ChunkID; }))
		{
			FChunkDependency Dep;
			Dep.ChunkID = i;
			Dep.ParentChunkID = 0;
			DependencyArray.Add(Dep);
		}
	}
	// Remove duplicates
	DependencyArray.StableSort([](const FChunkDependency& LHS, const FChunkDependency& RHS) { return LHS.ChunkID < RHS.ChunkID; });
	for (int32 i = 0; i < DependencyArray.Num() - 1;)
	{
		if (DependencyArray[i] == DependencyArray[i + 1])
		{
			DependencyArray.RemoveAt(i + 1);
		}
		else
		{
			++i;
		}
	}

	AddChildrenRecursive(RootTreeNode, DependencyArray, TSet<int32>());
	return &RootTreeNode;
}

void UChunkDependencyInfo::AddChildrenRecursive(FChunkDependencyTreeNode& Node, TArray<FChunkDependency>& DepInfo, TSet<int32> Parents)
{
	if (Parents.Num() > 0)
	{
		ChildToParentMap.FindOrAdd(Node.ChunkID).Append(Parents);
	}

	Parents.Add(Node.ChunkID);
	auto ChildNodeIndices = DepInfo.FilterByPredicate(
		[&](const FChunkDependency& RHS) 
		{
			return Node.ChunkID == RHS.ParentChunkID;
		});
	for (const auto& ChildIndex : ChildNodeIndices)
	{
		Node.ChildNodes.Add(FChunkDependencyTreeNode(ChildIndex.ChunkID));
	}
	for (auto& Child : Node.ChildNodes) 
	{
		AddChildrenRecursive(Child, DepInfo, Parents);
	}
}

void UChunkDependencyInfo::RemoveRedundantChunks(TArray<int32>& ChunkIDs) const
{
	for (int32 ChunkIndex = ChunkIDs.Num() - 1; ChunkIndex >= 0; ChunkIndex--)
	{
		const TSet<int32>* FoundParents = ChildToParentMap.Find(ChunkIDs[ChunkIndex]);

		if (FoundParents)
		{
			for (int32 ParentChunk : *FoundParents)
			{
				if (ChunkIDs.Contains(ParentChunk))
				{
					ChunkIDs.RemoveAt(ChunkIndex);
					break;
				}
			}
		}
	}
}

const FChunkDependencyTreeNode* LowestCommonAncestor(const FChunkDependencyTreeNode* RootNode, TSet<int32>& FoundSet, const TSet<int32>& FindSet)
{
	if (RootNode)
	{
		for (const FChunkDependencyTreeNode& ChildNode : RootNode->ChildNodes)
		{
			if (const FChunkDependencyTreeNode* ReturnNode = LowestCommonAncestor(&ChildNode, FoundSet, FindSet))
			{
				return ReturnNode;
			}
		}

		FoundSet.Add(RootNode->ChunkID);

		for (int32 Item : FindSet)
		{
			if (!FoundSet.Contains(Item))
			{
				return nullptr;
			}
		}

		return RootNode;
	}
	return nullptr;
}


int32 UChunkDependencyInfo::FindHighestSharedChunk(const TArray<int32>& ChunkIDs) const
{
	if (ChunkIDs.Num() == 0)
	{
		return 0;
	}
	if (ChunkIDs.Num() == 1)
	{
		return ChunkIDs[0];
	}

	TSet<int32> FoundSet;
	TSet<int32> FindSet;
	FindSet.Append(ChunkIDs);
	if (const FChunkDependencyTreeNode* BestParent = LowestCommonAncestor(&RootTreeNode, FoundSet, FindSet))
	{
		return BestParent->ChunkID;
	}
	else
	{
		UE_LOG(LogChunkDependencyInfo, Error, TEXT("Unable to find parent."));
		return 0;
	}
}
