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
using FKDTreeImplementationBase = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<float, FKDTree::FDataSource>, FKDTree::FDataSource, -1, AccessorType>;
struct FKDTreeImplementation : FKDTreeImplementationBase
{
	using FKDTreeImplementationBase::FKDTreeImplementationBase;

	bool operator==(const FKDTreeImplementation& Other) const
	{
		if (size_ != Other.size_)
		{
			return false;
		}

		if (dim_ != Other.dim_)
		{
			return false;
		}

		const AccessorType RootBBoxSize = root_bbox_.size();
		if (RootBBoxSize != Other.root_bbox_.size())
		{
			return false;
		}

		for (AccessorType Index = 0; Index < RootBBoxSize; ++Index)
		{
			const Interval& ThisInterval = root_bbox_[Index];
			const Interval& OtherInterval = Other.root_bbox_[Index];

			if (ThisInterval.high != OtherInterval.high)
			{
				return false;
			}

			if (ThisInterval.low != OtherInterval.low)
			{
				return false;
			}
		}

		if (leaf_max_size_ != Other.leaf_max_size_)
		{
			return false;
		}

		if (vAcc_ != Other.vAcc_)
		{
			return false;
		}

		if (!CompareNodes(root_node_, Other.root_node_))
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

FKDTree::FKDTree(AccessorType Count, AccessorType Dim, const float* Data, AccessorType MaxLeafSize)
: DataSource(Count, Dim, Data)
, KDTreeImplementation(nullptr)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	if (Count > 0 && Dim > 0 && Data)
	{
		KDTreeImplementation = new FKDTreeImplementation(Dim, DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(MaxLeafSize));
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN
}

FKDTree::FKDTree()
: DataSource(0, 0, nullptr)
, KDTreeImplementation(nullptr)
{
}

FKDTree::~FKDTree()
{
	Reset();
}

#if UE_POSE_SEARCH_USE_NANOFLANN
void CopySubTree(FKDTree& KDTree, FKDTreeImplementation::NodePtr& ThisNode, const FKDTreeImplementation::NodePtr& OtherNode)
{
	check(KDTree.KDTreeImplementation);

	ThisNode = KDTree.KDTreeImplementation->pool_.template allocate<FKDTreeImplementation::Node>();
	
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
	if (this != &Other && Other.KDTreeImplementation)
	{
		KDTreeImplementation = new FKDTreeImplementation(0, DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(0));

		DataSource = Other.DataSource;

		check(Other.KDTreeImplementation->size_ <= AccessorTypeMax);
		KDTreeImplementation->size_ = Other.KDTreeImplementation->size_;

		if (KDTreeImplementation->size_ > 0)
		{
			KDTreeImplementation->dim_ = Other.KDTreeImplementation->dim_;

			check(Other.KDTreeImplementation->root_bbox_.size() <= AccessorTypeMax);
			const AccessorType root_bbox_size = Other.KDTreeImplementation->root_bbox_.size();
			KDTreeImplementation->root_bbox_.resize(root_bbox_size);

			for (AccessorType i = 0; i < root_bbox_size; ++i)
			{
				KDTreeImplementation->root_bbox_[i] = Other.KDTreeImplementation->root_bbox_[i];
			}

			check(Other.KDTreeImplementation->leaf_max_size_ <= AccessorTypeMax);
			const AccessorType KDTreeLeafMaxSize = Other.KDTreeImplementation->leaf_max_size_;
			KDTreeImplementation->leaf_max_size_ = KDTreeLeafMaxSize;

			check(Other.KDTreeImplementation->vAcc_.size() <= AccessorTypeMax);
			const AccessorType VAccSize = Other.KDTreeImplementation->vAcc_.size();
			KDTreeImplementation->vAcc_.resize(VAccSize);
			
			for (AccessorType i = 0; i < VAccSize; ++i)
			{
				KDTreeImplementation->vAcc_[i] = Other.KDTreeImplementation->vAcc_[i];
			}
			
			CopySubTree(*this, KDTreeImplementation->root_node_, Other.KDTreeImplementation->root_node_);
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
	const bool bAnyImpl = KDTreeImplementation != nullptr;
	const bool bAnyOtherImpl = Other.KDTreeImplementation != nullptr;
	if (bAnyImpl != bAnyOtherImpl)
	{
		return false;
	}

#if UE_POSE_SEARCH_USE_NANOFLANN
	if (bAnyImpl && bAnyOtherImpl)
	{
		if (*KDTreeImplementation != *Other.KDTreeImplementation)
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
	delete KDTreeImplementation;
	KDTreeImplementation = nullptr;
#endif // UE_POSE_SEARCH_USE_NANOFLANN
	DataSource = FDataSource();
}

void FKDTree::Construct(AccessorType Count, AccessorType Dim, const float* Data, AccessorType MaxLeafSize)
{
	Reset();
	new(this)FKDTree(Count, Dim, Data, MaxLeafSize);
}

template <typename RESULTSET>
inline int32 FindNeighborsInternal(FKDTreeImplementation* KDTreeImplementation, RESULTSET& Result, TConstArrayView<float> Query)
{
#if UE_POSE_SEARCH_USE_NANOFLANN

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FKDTree_FindNeighbors);

	check(Query.GetData() && Query.Num() == KDTreeImplementation->dim_ && KDTreeImplementation->root_node_);

	const nanoflann::SearchParameters SearchParameters(
		0.f,		// search for eps-approximate neighbours (default: 0)
		false);		// only for radius search, require neighbours sorted by
	KDTreeImplementation->findNeighbors(Result, Query.GetData(), SearchParameters);
	return Result.Num();

#else // UE_POSE_SEARCH_USE_NANOFLANN

	checkNoEntry(); // unimplemented
	return 0;

#endif // UE_POSE_SEARCH_USE_NANOFLANN
}

int32 FKDTree::FindNeighbors(FKNNResultSet& Result, TConstArrayView<float> Query) const
{
	return FindNeighborsInternal(KDTreeImplementation, Result, Query);
}

int32 FKDTree::FindNeighbors(FFilteredKNNResultSet& Result, TConstArrayView<float> Query) const
{
	return FindNeighborsInternal(KDTreeImplementation, Result, Query);
}

int32 FKDTree::FindNeighbors(FRadiusResultSet& Result, TConstArrayView<float> Query) const
{
	return FindNeighborsInternal(KDTreeImplementation, Result, Query);
}

SIZE_T FKDTree::GetAllocatedSize() const
{
	SIZE_T AllocatedSize = sizeof(FKDTree);

#if UE_POSE_SEARCH_USE_NANOFLANN
	if (KDTreeImplementation)
	{
		AllocatedSize += sizeof(FKDTreeImplementation);
		AllocatedSize += KDTreeImplementation->usedMemory(*KDTreeImplementation);
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN

	return AllocatedSize;
}

#if UE_POSE_SEARCH_USE_NANOFLANN
FArchive& SerializeSubTree(FArchive& Ar, FKDTree& KDTree, FKDTreeImplementation::NodePtr& KDTreeNode)
{
	check(KDTree.KDTreeImplementation);

	if (Ar.IsLoading())
	{
		KDTreeNode = KDTree.KDTreeImplementation->pool_.template allocate<FKDTreeImplementation::Node>();
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
		check(KDTreeNode->node_type.lr.left <= AccessorTypeMax);
		check(KDTreeNode->node_type.lr.right <= AccessorTypeMax);

		AccessorType OffsetLeft = KDTreeNode->node_type.lr.left;
		AccessorType OffsetRight = KDTreeNode->node_type.lr.right;

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
	check(!KDTree.KDTreeImplementation || KDTree.KDTreeImplementation->size_ <= AccessorTypeMax);

	AccessorType KDTreeSize = KDTree.KDTreeImplementation ? KDTree.KDTreeImplementation->size_ : 0;

	Ar << KDTreeSize;

	if (KDTreeSize > 0)
	{
		if (Ar.IsLoading() && !KDTree.KDTreeImplementation)
		{
			KDTree.KDTreeImplementation = new FKDTreeImplementation(0, KDTree.DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(0));
		}

		KDTree.KDTreeImplementation->size_ = KDTreeSize;

		Ar << KDTree.KDTreeImplementation->dim_;

		AccessorType root_bbox_size = KDTree.KDTreeImplementation->root_bbox_.size();
		check(KDTree.KDTreeImplementation->root_bbox_.size() <= AccessorTypeMax);
		Ar << root_bbox_size;

		if (Ar.IsLoading())
		{
			KDTree.DataSource.Data = KDTreeData;
			KDTree.DataSource.PointDim = KDTree.KDTreeImplementation->dim_;
			KDTree.DataSource.PointCount = KDTree.KDTreeImplementation->size_;

			KDTree.KDTreeImplementation->root_bbox_.resize(root_bbox_size);
		}

		for (FKDTreeImplementation::Interval& el : KDTree.KDTreeImplementation->root_bbox_)
		{
			Ar.Serialize(&el, sizeof(FKDTreeImplementation::Interval));
		}

		check(KDTree.KDTreeImplementation->leaf_max_size_ <= AccessorTypeMax);
		AccessorType KDTreeLeafMaxSize = KDTree.KDTreeImplementation->leaf_max_size_;
		Ar << KDTreeLeafMaxSize;
		KDTree.KDTreeImplementation->leaf_max_size_ = KDTreeLeafMaxSize;

		check(KDTree.KDTreeImplementation->vAcc_.size() <= AccessorTypeMax);
		AccessorType VAccSize = KDTree.KDTreeImplementation->vAcc_.size();
		Ar << VAccSize;
		if (Ar.IsLoading())
		{
			KDTree.KDTreeImplementation->vAcc_.resize(VAccSize);
		}
		for (AccessorType& el : KDTree.KDTreeImplementation->vAcc_)
		{
			Ar << el;
		}
		SerializeSubTree(Ar, KDTree, KDTree.KDTreeImplementation->root_node_);
	}
	else if (Ar.IsLoading())
	{
		KDTree.Reset();
	}
#endif // UE_POSE_SEARCH_USE_NANOFLANN

	return Ar;
}

} // namespace UE::PoseSearch
