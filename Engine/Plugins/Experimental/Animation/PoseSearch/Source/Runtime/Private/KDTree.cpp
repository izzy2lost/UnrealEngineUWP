// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/KDTree.h"
#include "Stats/Stats.h"

#ifndef UE_POSE_SEARCH_USE_NANOFLANN
	#define UE_POSE_SEARCH_USE_NANOFLANN 1
#endif

// @third party code - BEGIN nanoflann
#if UE_POSE_SEARCH_USE_NANOFLANN
THIRD_PARTY_INCLUDES_START
#include "nanoflann/nanoflann.hpp"
THIRD_PARTY_INCLUDES_END
#endif
// @third party code - END nanoflann

namespace UE::PoseSearch
{

#if UE_POSE_SEARCH_USE_NANOFLANN
using FKDTreeImplementationBase = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<float, FKDTree::FDataSource>, FKDTree::FDataSource>;
struct FKDTreeImplementation : FKDTreeImplementationBase
{
	using FKDTreeImplementationBase::FKDTreeImplementationBase;

	bool operator==(const FKDTreeImplementation& Other) const
	{
		if (m_size != Other.m_size)
		{
			return false;
		}

		if (dim != Other.dim)
		{
			return false;
		}

		const uint32 RootBBoxSize = root_bbox.size();
		if (RootBBoxSize != Other.root_bbox.size())
		{
			return false;
		}

		for (uint32 Index = 0; Index < RootBBoxSize; ++Index)
		{
			const Interval& ThisInterval = root_bbox[Index];
			const Interval& OtherInterval = Other.root_bbox[Index];

			if (ThisInterval.high != OtherInterval.high)
			{
				return false;
			}

			if (ThisInterval.low != OtherInterval.low)
			{
				return false;
			}
		}

		if (m_leaf_max_size != Other.m_leaf_max_size)
		{
			return false;
		}

		if (vAcc != Other.vAcc)
		{
			return false;
		}

		if (!CompareNodes(root_node, Other.root_node))
		{
			return false;
		}

		return true;
	}

private:
	static bool CompareNodes(const NodePtr& NodeA, const NodePtr& NodeB)
	{
		const bool bAnyNodeAChild1 = NodeA->child1 != nullptr;
		const bool bAnyNodeBChild1 = NodeA->child1 != nullptr;
		if (bAnyNodeAChild1 != bAnyNodeBChild1)
		{
			return false;
		}

		const bool bAnyNodeAChild2 = NodeA->child2 != nullptr;
		const bool bAnyNodeBChild2 = NodeA->child2 != nullptr;
		if (bAnyNodeAChild2 != bAnyNodeBChild2)
		{
			return false;
		}

		const bool bIsLeafNode = !bAnyNodeAChild1 && !bAnyNodeAChild2;
		if (bIsLeafNode)
		{
			if (NodeA->node_type.lr.left != NodeB->node_type.lr.left)
			{
				return false;
			}

			if (NodeA->node_type.lr.right != NodeB->node_type.lr.right)
			{
				return false;
			}
		}
		else
		{
			if (NodeA->node_type.sub.divfeat != NodeB->node_type.sub.divfeat)
			{
				return false;
			}

			if (NodeA->node_type.sub.divhigh != NodeB->node_type.sub.divhigh)
			{
				return false;
			}

			if (NodeA->node_type.sub.divlow != NodeB->node_type.sub.divlow)
			{
				return false;
			}
		}

		if (bAnyNodeAChild1)
		{
			if (!CompareNodes(NodeA->child1, NodeB->child1))
			{
				return false;
			}
		}

		if (bAnyNodeAChild2)
		{
			if (!CompareNodes(NodeA->child2, NodeB->child2))
			{
				return false;
			}
		}

		return true;
	}
};
#endif

FKDTree::FKDTree(int32 Count, int32 Dim, const float* Data, int32 MaxLeafSize)
: DataSource(Count, Dim, Data)
, Impl(nullptr)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	if (Count > 0 && Dim > 0 && Data)
	{
		Impl = new FKDTreeImplementation(Dim, DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(MaxLeafSize));
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN
}

FKDTree::FKDTree()
: DataSource(0, 0, nullptr)
, Impl(nullptr)
{
}

FKDTree::~FKDTree()
{
	Reset();
}

#if UE_POSE_SEARCH_USE_NANOFLANN
void CopySubTree(FKDTree& KDTree, FKDTreeImplementation::NodePtr& ThisNode, const FKDTreeImplementation::NodePtr& OtherNode)
{
	check(KDTree.Impl);

	ThisNode = KDTree.Impl->pool.template allocate<FKDTreeImplementation::Node>();
	
	ThisNode->node_type = OtherNode->node_type;

	if (OtherNode->child1 != nullptr)
	{
		CopySubTree(KDTree, ThisNode->child1, OtherNode->child1);
	}
	else
	{
		ThisNode->child1 = nullptr;
	}

	if (OtherNode->child2 != nullptr)
	{
		CopySubTree(KDTree, ThisNode->child2, OtherNode->child2);
	}
	else
	{
		ThisNode->child2 = nullptr;
	}
}

#endif // UE_POSE_SEARCH_USE_NANOFLANN

FKDTree::FKDTree(const FKDTree& Other)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	if (this != &Other && Other.Impl)
	{
		Impl = new FKDTreeImplementation(0, DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(0));

		DataSource = Other.DataSource;

		check(Other.Impl->m_size <= UINT_MAX);
		Impl->m_size = Other.Impl->m_size;

		if (Impl->m_size > 0)
		{
			Impl->dim = Other.Impl->dim;

			check(Other.Impl->root_bbox.size() <= UINT_MAX);
			const uint32 root_bbox_size = Other.Impl->root_bbox.size();
			Impl->root_bbox.resize(root_bbox_size);

			for (uint32 i = 0; i < root_bbox_size; ++i)
			{
				Impl->root_bbox[i] = Other.Impl->root_bbox[i];
			}

			check(Other.Impl->m_leaf_max_size <= UINT_MAX);
			const uint32 KDTreeLeafMaxSize = Other.Impl->m_leaf_max_size;
			Impl->m_leaf_max_size = KDTreeLeafMaxSize;

			check(Other.Impl->vAcc.size() <= UINT_MAX);
			const uint32 VAccSize = Other.Impl->vAcc.size();
			Impl->vAcc.resize(VAccSize);
			
			for (uint32 i = 0; i < VAccSize; ++i)
			{
				Impl->vAcc[i] = Other.Impl->vAcc[i];
			}
			
			CopySubTree(*this, Impl->root_node, Other.Impl->root_node);
		}
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN
}

FKDTree& FKDTree::operator=(const FKDTree& Other)
{
	if (this != &Other)
	{
		Reset();
		new(this)FKDTree(Other);
	}
	return *this;
}

bool FKDTree::operator==(const FKDTree& Other) const
{
	const bool bAnyImpl = Impl != nullptr;
	const bool bAnyOtherImpl = Other.Impl != nullptr;
	if (bAnyImpl != bAnyOtherImpl)
	{
		return false;
	}

#if UE_POSE_SEARCH_USE_NANOFLANN
	if (bAnyImpl && bAnyOtherImpl)
	{
		if (*Impl != *Other.Impl)
		{
			return false;
		}
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN

	if (DataSource != Other.DataSource)
	{
		return false;
	}

	return true;
}

void FKDTree::Reset()
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	delete Impl;
	Impl = nullptr;
#endif // UE_POSE_SEARCH_USE_NANOFLANN
	DataSource = FDataSource();
}

void FKDTree::Construct(int32 Count, int32 Dim, const float* Data, int32 MaxLeafSize)
{
	Reset();
	new(this)FKDTree(Count, Dim, Data, MaxLeafSize);
}

bool FKDTree::FindNeighbors(FKNNResultSet& Result, TConstArrayView<float> Query) const
{
#if UE_POSE_SEARCH_USE_NANOFLANN

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FKDTree_FindNeighbors);

	check(Query.GetData() && Query.Num() == Impl->dim && Impl->root_node);

	const nanoflann::SearchParams SearchParams(
		32,			// Ignored parameter (Kept for compatibility with the FLANN interface).
		0.f,		// search for eps-approximate neighbours (default: 0)
		false);		// only for radius search, require neighbours sorted by
	return Impl->findNeighbors(Result, Query.GetData(), SearchParams);

#else // UE_POSE_SEARCH_USE_NANOFLANN

	checkNoEntry(); // unimplemented
	return false;

#endif // UE_POSE_SEARCH_USE_NANOFLANN
}

bool FKDTree::FindNeighbors(FRadiusResultSet& Result, TConstArrayView<float> Query) const
{
#if UE_POSE_SEARCH_USE_NANOFLANN

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FKDTree_FindNeighbors);

	check(Query.GetData() && Query.Num() == Impl->dim && Impl->root_node);

	const nanoflann::SearchParams SearchParams(
		32,			// Ignored parameter (Kept for compatibility with the FLANN interface).
		0.f,		// search for eps-approximate neighbours (default: 0)
		false);		// only for radius search, require neighbours sorted by
	return Impl->findNeighbors(Result, Query.GetData(), SearchParams);

#else // UE_POSE_SEARCH_USE_NANOFLANN

	checkNoEntry(); // unimplemented
	return false;

#endif // UE_POSE_SEARCH_USE_NANOFLANN
}

SIZE_T FKDTree::GetAllocatedSize() const
{
	SIZE_T AllocatedSize = sizeof(FKDTree);

#if UE_POSE_SEARCH_USE_NANOFLANN
	if (Impl)
	{
		AllocatedSize += sizeof(FKDTreeImplementation);
		AllocatedSize += Impl->usedMemory(*Impl);
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN

	return AllocatedSize;
}

#if UE_POSE_SEARCH_USE_NANOFLANN
FArchive& SerializeSubTree(FArchive& Ar, FKDTree& KDTree, FKDTreeImplementation::NodePtr& KDTreeNode)
{
	check(KDTree.Impl);

	if (Ar.IsLoading())
	{
		KDTreeNode = KDTree.Impl->pool.template allocate<FKDTreeImplementation::Node>();
		// zeroing FKDTreeImplementation::Node memory since it contains a union and doesn't have a constructor 
		FMemory::Memzero(KDTreeNode, sizeof(FKDTreeImplementation::Node));
	}

	bool bAnyNodeChild1 = KDTreeNode->child1 != nullptr;
	bool bAnyNodeChild2 = KDTreeNode->child2 != nullptr;
	Ar << bAnyNodeChild1;
	Ar << bAnyNodeChild2;

	const bool bIsLeafNode = !bAnyNodeChild1 && !bAnyNodeChild2;
	if (bIsLeafNode)
	{
		check(KDTreeNode->node_type.lr.left <= UINT_MAX);
		check(KDTreeNode->node_type.lr.right <= UINT_MAX);

		uint32 OffsetLeft = KDTreeNode->node_type.lr.left;
		uint32 OffsetRight = KDTreeNode->node_type.lr.right;

		Ar << OffsetLeft;
		Ar << OffsetRight;

		KDTreeNode->node_type.lr.left = OffsetLeft;
		KDTreeNode->node_type.lr.right = OffsetRight;
	}
	else
	{
		Ar << KDTreeNode->node_type.sub.divfeat;
		Ar << KDTreeNode->node_type.sub.divhigh;
		Ar << KDTreeNode->node_type.sub.divlow;
	}

	if (bAnyNodeChild1)
	{
		SerializeSubTree(Ar, KDTree, KDTreeNode->child1);
	}
	else if (Ar.IsLoading())
	{
		KDTreeNode->child1 = nullptr;
	}

	if (bAnyNodeChild2)
	{
		SerializeSubTree(Ar, KDTree, KDTreeNode->child2);
	}
	else if (Ar.IsLoading())
	{
		KDTreeNode->child2 = nullptr;
	}
	return Ar;
}

#endif // UE_POSE_SEARCH_USE_NANOFLANN

FArchive& Serialize(FArchive& Ar, FKDTree& KDTree, const float* KDTreeData)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	check(!KDTree.Impl || KDTree.Impl->m_size <= UINT_MAX);

	uint32 KDTreeSize = KDTree.Impl ? KDTree.Impl->m_size : 0;

	Ar << KDTreeSize;

	if (KDTreeSize > 0)
	{
		if (Ar.IsLoading() && !KDTree.Impl)
		{
			KDTree.Impl = new FKDTreeImplementation(0, KDTree.DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(0));
		}

		KDTree.Impl->m_size = KDTreeSize;

		Ar << KDTree.Impl->dim;

		uint32 root_bbox_size = KDTree.Impl->root_bbox.size();
		check(KDTree.Impl->root_bbox.size() <= UINT_MAX);
		Ar << root_bbox_size;

		if (Ar.IsLoading())
		{
			KDTree.DataSource.Data = KDTreeData;
			KDTree.DataSource.PointDim = KDTree.Impl->dim;
			KDTree.DataSource.PointCount = KDTree.Impl->m_size;

			KDTree.Impl->root_bbox.resize(root_bbox_size);
		}

		for (FKDTreeImplementation::Interval& el : KDTree.Impl->root_bbox)
		{
			Ar.Serialize(&el, sizeof(FKDTreeImplementation::Interval));
		}

		check(KDTree.Impl->m_leaf_max_size <= UINT_MAX);
		uint32 KDTreeLeafMaxSize = KDTree.Impl->m_leaf_max_size;
		Ar << KDTreeLeafMaxSize;
		KDTree.Impl->m_leaf_max_size = KDTreeLeafMaxSize;

		check(KDTree.Impl->vAcc.size() <= UINT_MAX);
		uint32 VAccSize = KDTree.Impl->vAcc.size();
		Ar << VAccSize;
		if (Ar.IsLoading())
		{
			KDTree.Impl->vAcc.resize(VAccSize);
		}
		for (uint32_t& el : KDTree.Impl->vAcc)
		{
			Ar << el;
		}
		SerializeSubTree(Ar, KDTree, KDTree.Impl->root_node);
	}
	else if (Ar.IsLoading())
	{
		KDTree.Reset();
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN

	return Ar;
}

} // namespace UE::PoseSearch
