// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Misc/TVariant.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "OverrideVoidReturnInvoker.h"

struct IStaticSpatialIndexDataInterface
{
	virtual ~IStaticSpatialIndexDataInterface() {}
	virtual const FBox& GetBox(uint32 InIndex) const = 0;
	virtual uint32 GetAllocatedSize() const = 0;
};

template <typename ValueType, class SpatialIndexType, class ElementsSorter>
class TStaticSpatialIndex : public IStaticSpatialIndexDataInterface
{
public:
	TStaticSpatialIndex()
		: SpatialIndex(*this)
	{}

	void Init(const TArray<TPair<FBox, ValueType>>& InElements)
	{
		Elements = InElements;
		InitSpatialIndex();
	}

	void Init(TArray<TPair<FBox, ValueType>>&& InElements)
	{
		Elements = MoveTemp(InElements);
		InitSpatialIndex();
	}

	template <class Func>
	void ForEachElement(Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		SpatialIndex.ForEachElement([this, &Invoker](uint32 ValueIndex)
		{
			return Invoker(Elements[ValueIndex].Value);
		});
	}

	template <class Func>
	void ForEachIntersectingElement(const FBox& InBox, Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		SpatialIndex.ForEachIntersectingElement(InBox, [this, &Invoker](uint32 ValueIndex)
		{
			return Invoker(Elements[ValueIndex].Value);
		});
	}

	template <class Func>
	void ForEachIntersectingElement(const FSphere& InSphere, Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		SpatialIndex.ForEachIntersectingElement(InSphere, [this, &Invoker](uint32 ValueIndex)
		{
			return Invoker(Elements[ValueIndex].Value);
		});
	}

	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		if constexpr (TIsPointerOrObjectPtrToBaseOf<ValueType, UObject>::Value)
		{
			for (TPair<FBox, ValueType>& Element : Elements)
			{
				Collector.AddReferencedObject(Element.Value);
			}
		}
	}

	// IStaticSpatialIndexDataInterface interface
	virtual const FBox& GetBox(uint32 InIndex) const override
	{
		return Elements[InIndex].Key;
	}

	virtual uint32 GetAllocatedSize() const override
	{
		return sizeof(*this) + Elements.GetAllocatedSize() + SpatialIndex.GetAllocatedSize();
	}

private:
	void InitSpatialIndex()
	{
		// Sort elements to maximize cache coherency during queries
		if (ElementsSorter::NeedSort)
		{
			FBox ElementsBox;
			Algo::ForEach(Elements, [&ElementsBox](const TPair<FBox, ValueType>& Element) { ElementsBox += Element.Key; });

			ElementsSorter Sorter;
			Sorter.Init(ElementsBox);
			Elements.Sort([&Sorter](const TPair<FBox, ValueType>& A, const TPair<FBox, ValueType>B) { return Sorter.Sort(A.Key, B.Key); });
		}

		// Build index data for type agnostic implementation
		TArray<TPair<FBox, uint32>> IndexData;
		
		int32 ElemIndex = 0;
		IndexData.Reserve(Elements.Num());
		Algo::Transform(Elements, IndexData, [&ElemIndex](const TPair<FBox, ValueType>& Element) { return TPair<FBox, uint32>(Element.Key, ElemIndex++); });

		// Initialize spatial index implementation
		SpatialIndex.Init(IndexData);
	}

	TArray<TPair<FBox, ValueType>> Elements;
	SpatialIndexType SpatialIndex;
};

namespace FStaticSpatialIndex
{
	class FImpl
	{
	public:
		FImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: DataInterface(InDataInterface)
		{}

	protected:
		const IStaticSpatialIndexDataInterface& DataInterface;
	};

	class FListImpl : public FImpl
	{
	public:
		FListImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: FImpl(InDataInterface)
		{}

		void Init(const TArray<TPair<FBox, uint32>>& InElements);

		bool ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;
		bool ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;
		bool ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;

		uint32 GetAllocatedSize() const;

	private:
		TArray<uint32> Elements;
	};

	class FRTreeBaseImpl : public FImpl
	{
	public:
		FRTreeBaseImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: FImpl(InDataInterface)
		{}

		bool ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;
		bool ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;
		bool ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;

		uint32 GetAllocatedSize() const;

	protected:
		struct FNode
		{
			using FNodeType = TArray<FNode>;
			using FLeafType = TArray<uint32>;

			FBox Box = FBox(ForceInit);
			TVariant<FNodeType, FLeafType> Content;
		};

		bool ForEachElementRecursive(const FNode* InNode, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;
		bool ForEachIntersectingElementRecursive(const FNode* InNode, const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;
		bool ForEachIntersectingElementRecursive(const FNode* InNode, const FVector& InSphereCenter, FSphere::FReal InRadiusSquared, TFunctionRef<bool(uint32 InValueIndex)> InFunc) const;

		FNode RootNode;
	};

	template <int32 MaxNumElementsPerNode = 64, int32 MaxNumElementsPerLeaf = 64>
	class FRTreeImpl : public FRTreeBaseImpl
	{
	public:
		FRTreeImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: FRTreeBaseImpl(InDataInterface)
		{}

		void Init(const TArray<TPair<FBox, uint32>>& InElements)
		{
			if (InElements.Num())
			{
				// build leaves
				FNode* CurrentNode = nullptr;
				TArray<FNode> Nodes;

				for (const TPair<FBox, uint32>& Element : InElements)
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
	};

	class FNodeSorterNoSort
	{
	public:
		enum { NeedSort = 0 };
		void Init(const FBox& SortBox) {}
		bool Sort(const FBox& A, const FBox& B) { return false; }
	};

	class FNodeSorterMinX
	{
	public:
		enum { NeedSort = 1 };

		void Init(const FBox& SortBox) {}
		bool Sort(const FBox& A, const FBox& B) { return A.Min.X < B.Min.X; }
	};

	template <uint32 BucketSize>
	class TNodeSorterMorton
	{
	public:
		enum { NeedSort = 1 };

		void Init(const FBox& SortBox)
		{}
		
		bool Sort(const FBox& A, const FBox& B)
		{
			const uint32 MortonCodeA = MortonEncode({ int32(A.GetCenter().X / (FBox::FReal)BucketSize), int32(A.GetCenter().Y / (FBox::FReal)BucketSize), int32(A.GetCenter().Z / (FBox::FReal)BucketSize) });
			const uint32 MortonCodeB = MortonEncode({ int32(B.GetCenter().X / (FBox::FReal)BucketSize), int32(B.GetCenter().Y / (FBox::FReal)BucketSize), int32(B.GetCenter().Z / (FBox::FReal)BucketSize) });
			return MortonCodeA < MortonCodeB;
		}

	private:
		inline uint32 MortonEncode(const FInt32Vector3& Point)
		{
			return FMath::MortonCode3(Point.X) | (FMath::MortonCode3(Point.Y) << 1) | (FMath::MortonCode3(Point.Z) << 2);
		}
	};

	template <uint32 BucketSize>
	class TNodeSorterHilbert
	{
	public:
		enum { NeedSort = 1 };

		void Init(const FBox& SortBox)
		{
			const FBox::FReal MaxExtent = SortBox.GetExtent().GetMax();
			const uint32 NumBuckets = FMath::CeilToInt32(MaxExtent / (FBox::FReal)BucketSize);			
			HilbertOrder = 1 + FMath::FloorLog2(NumBuckets);
		}
		
		bool Sort(const FBox& A, const FBox& B)
		{
			const int32 HilbertCodeA = HilbertEncode({ int32(A.GetCenter().X / (FBox::FReal)BucketSize), int32(A.GetCenter().Y / (FBox::FReal)BucketSize) }, HilbertOrder);
			const int32 HilbertCodeB = HilbertEncode({ int32(B.GetCenter().X / (FBox::FReal)BucketSize), int32(B.GetCenter().Y / (FBox::FReal)BucketSize) }, HilbertOrder);
			return HilbertCodeA < HilbertCodeB;
		}

	private:
		uint32 HilbertEncode(FIntPoint Point, uint32 Order)
		{
			uint32 Result = 0;

			uint32 State = 0;
			for (int32 i = Order - 1; i >= 0; i--)
			{
				uint32 Row = 4 * State | 2 * ((Point.X >> i) & 1) | ((Point.Y >> i) & 1);
				Result = (Result << 2) | ((0x361e9cb4 >> 2 * Row) & 3);
				State = (0x8fe65831 >> 2 * Row) & 3;
			}

			return Result;
		}

		uint32 HilbertOrder;
	};
}

template <class ValueType, class NodeSorter> class TStaticSpatialIndexList : public TStaticSpatialIndex<ValueType, FStaticSpatialIndex::FListImpl, NodeSorter> {};
template <class ValueType, class NodeSorter> class TStaticSpatialIndexRTree : public TStaticSpatialIndex<ValueType, FStaticSpatialIndex::FRTreeImpl<16, 64>, NodeSorter> {};