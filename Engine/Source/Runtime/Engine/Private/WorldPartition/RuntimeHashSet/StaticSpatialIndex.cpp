// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#include "Algo/Transform.h"

namespace FStaticSpatialIndex
{
	inline bool FastSphereAABBIntersection(const FVector& InSphereCenter, const FSphere::FReal InRadiusSquared, const FBox& InAABB)
	{
		const FVector ClosestPoint = FVector::Max(InAABB.Min, FVector::Min(InSphereCenter, InAABB.Max));
		return (ClosestPoint - InSphereCenter).SizeSquared() <= InRadiusSquared;
	}
}

void FStaticSpatialIndex::FListImpl::Init(const TArray<TPair<FBox, uint32>>& InElements)
{
	Elements.Reserve(InElements.Num());
	Algo::Transform(InElements, Elements, [](const TPair<FBox, uint32>& Element) { return Element.Value; });
}

bool FStaticSpatialIndex::FListImpl::ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	for (uint32 ValueIndex : Elements)
	{
		if (!InFunc(ValueIndex))
		{
			return false;
		}
	}
	return true;
}

bool FStaticSpatialIndex::FListImpl::ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	for (uint32 ValueIndex : Elements)
	{
		const FBox& Box = DataInterface.GetBox(ValueIndex);

		if (Box.Intersect(InBox))
		{
			if (!InFunc(ValueIndex))
			{
				return false;
			}
		}
	}
	return true;
}

bool FStaticSpatialIndex::FListImpl::ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	const FSphere::FReal RadiusSquared = FMath::Square(InSphere.W);

	for (uint32 ValueIndex : Elements)
	{
		const FBox& Box = DataInterface.GetBox(ValueIndex);

		if (FastSphereAABBIntersection(InSphere.Center, RadiusSquared, Box))
		{
			if (!InFunc(ValueIndex))
			{
				return false;
			}
		}
	}
	return true;
}

uint32 FStaticSpatialIndex::FListImpl::GetAllocatedSize() const
{
	return sizeof(*this) + Elements.GetAllocatedSize();
}

void FStaticSpatialIndex::FRTreeImpl::Init(const TArray<TPair<FBox, uint32>>& InElements)
{
	if (InElements.Num())
	{
		const int32 MaxNumElementsPerNode = 16;
		const int32 MaxNumElementsPerLeaf = 64;

		TArray<TPair<FBox, uint32>> SortedElements = InElements;
		SortedElements.Sort([](const TPair<FBox, uint32>& A, const TPair<FBox, uint32>& B) { return A.Key.Min.X < B.Key.Min.X; });
	
		// build leaves
		FNode* CurrentNode = nullptr;
		TArray<FNode> Nodes;

		for (const TPair<FBox, uint32>& Element : SortedElements)
		{
			if (!CurrentNode || (CurrentNode->Content.Get<FNode::FLeafType>().Num() >= MaxNumElementsPerLeaf))
			{
				CurrentNode = &Nodes.Emplace_GetRef();
				CurrentNode->Content.Emplace<FNode::FLeafType>();
			}

			CurrentNode->Box += Element.Key;
			CurrentNode->Content.Get<FNode::FLeafType>().Add(Element.Value);
		}

		// build nodes
		while (Nodes.Num() > 1)
		{
			CurrentNode = nullptr;
			TArray<FNode> TopNodes;

			Nodes.Sort([](const FNode& A, const FNode& B) { return A.Box.Min.X < B.Box.Min.X; });

			for (FNode& Node : Nodes)
			{
				if (!CurrentNode || (CurrentNode->Content.Get<FNode::FNodeType>().Num() >= MaxNumElementsPerNode))
				{
					CurrentNode = &TopNodes.Emplace_GetRef();
					CurrentNode->Content.Emplace<FNode::FNodeType>();
				}

				CurrentNode->Box += Node.Box;
				CurrentNode->Content.Get<FNode::FNodeType>().Add(MoveTemp(Node));
			}

			Nodes = MoveTemp(TopNodes);
		}

		check(Nodes.Num() == 1);
		RootNode = MoveTemp(Nodes[0]);
	}
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	return ForEachElementRecursive(&RootNode, InFunc);
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachElementRecursive(const FNode* Node, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			if (!ForEachElementRecursive(&ChildNode, InFunc))
			{
				return false;
			}
		}
	}
	else
	{
		for (uint32 ValueIndex : Node->Content.Get<FNode::FLeafType>())
		{
			if (!InFunc(ValueIndex))
			{
				return false;
			}
		}
	}
	return true;
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	return ForEachIntersectingElementRecursive(&RootNode, InBox, InFunc);
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElementRecursive(const FNode* InNode, const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	if (InNode->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : InNode->Content.Get<FNode::FNodeType>())
		{
			if (ChildNode.Box.Intersect(InBox))
			{
				if (!ForEachIntersectingElementRecursive(&ChildNode, InBox, InFunc))
				{
					return false;
				}
			}
		}
	}
	else
	{
		for (uint32 ValueIndex : InNode->Content.Get<FNode::FLeafType>())
		{
			const FBox& Box = DataInterface.GetBox(ValueIndex);

			if (Box.Intersect(InBox))
			{
				if (!InFunc(ValueIndex))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	const FSphere::FReal RadiusSquared = FMath::Square(InSphere.W);
	return ForEachIntersectingElementRecursive(&RootNode, InSphere.Center, RadiusSquared, InFunc);
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElementRecursive(const FNode* InNode, const FVector& InSphereCenter, FSphere::FReal InRadiusSquared, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const
{
	if (InNode->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : InNode->Content.Get<FNode::FNodeType>())
		{
			if (FastSphereAABBIntersection(InSphereCenter, InRadiusSquared, ChildNode.Box))
			{
				if (!ForEachIntersectingElementRecursive(&ChildNode, InSphereCenter, InRadiusSquared, InFunc))
				{
					return false;
				}
			}
		}
	}
	else
	{
		for (uint32 ValueIndex : InNode->Content.Get<FNode::FLeafType>())
		{
			const FBox& Box = DataInterface.GetBox(ValueIndex);

			if (FastSphereAABBIntersection(InSphereCenter, InRadiusSquared, Box))
			{
				if (!InFunc(ValueIndex))
				{
					return false;
				}
			}
		}
	}
	return true;
}

uint32 FStaticSpatialIndex::FRTreeImpl::GetAllocatedSize() const
{
	TFunction<uint32(const FNode*, uint32)> GetAllocatedSizeRecursive = [this, &GetAllocatedSizeRecursive](const FNode* Node, uint32 BaseSize)
	{
		uint32 AllocatedSize = BaseSize;

		if (Node->Content.IsType<FNode::FNodeType>())
		{
			AllocatedSize += Node->Content.Get<FNode::FNodeType>().GetAllocatedSize();

			for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
			{
				AllocatedSize += GetAllocatedSizeRecursive(&ChildNode, sizeof(FNode));
			}
		}
		else
		{
			AllocatedSize += Node->Content.Get<FNode::FLeafType>().GetAllocatedSize();
		}

		return AllocatedSize;
	};
	
	return sizeof(*this) + GetAllocatedSizeRecursive(&RootNode, 0);
}
