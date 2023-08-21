// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include "Containers/PagedArray.h"
#include "Utils.h"

namespace AutoRTFM
{

struct FIntervalTree final
{
    FIntervalTree() = default;

    FIntervalTree(const FIntervalTree&) = delete;
    FIntervalTree& operator=(const FIntervalTree&) = delete;

	FORCENOINLINE bool Insert(const void* const Address, const size_t Size)
    {
        FRange NewRange(Address, Size);

        if (UNLIKELY(IntervalTreeNodeIndexNone == Root))
        {
            ASSERT(0 == Nodes.Num());
            Nodes.Add(FIntervalTreeNode(NewRange));
            Root = 0;
            return true;
        }

		ASSERT(Nodes[Root].Parent == IntervalTreeNodeIndexNone);

        FIntervalTreeNodeIndex Current = Root;

        for(;;)
        {
            FIntervalTreeNode& Node = Nodes[Current];

            const FRange Range = Node.Range;

            if (UNLIKELY((NewRange.Start < Range.End) && (Range.Start < NewRange.End)))
            {
                return false;
            }

            if (NewRange.Start < Range.Start)
            {
				if (NewRange.End == Range.Start)
				{
                    // We can just modify the existing node in place.
					ASSERT(NewRange.Start < Range.Start);
					Node.Range.Start = NewRange.Start;
                    return true;
				}
				else if (IntervalTreeNodeIndexNone == Node.Left)
                {
                    const FIntervalTreeNodeIndex Index = Nodes.Add(FIntervalTreeNode(NewRange, Current));
                    Node.Left = Index;
                    Current = Index;
                    break;
                }

                Current = Node.Left;
            }
            else
            {
				if (NewRange.Start == Range.End)
				{
                    // We can just modify the existing node in place.
					ASSERT(NewRange.End > Range.End);
					Node.Range.End = NewRange.End;
					return true;
				}
				else if (IntervalTreeNodeIndexNone == Node.Right)
                {
                    const FIntervalTreeNodeIndex Index = Nodes.Add(FIntervalTreeNode(NewRange, Current));
                    Node.Right = Index;
                    Current = Index;
                    break;
                }

                Current = Node.Right;
            }

			ASSERT(Root != Current);
        }

        auto IsBlack = [this](FIntervalTreeNodeIndex Index)
        {
            return IntervalTreeNodeIndexNone == Index || Nodes[Index].bIsBlack;
        };

        for(;;)
        {
            FIntervalTreeNodeIndex Parent = Nodes[Current].Parent;

            UE_ASSUME(Current != Parent);

            // The root will always have a black parent, so this check covers both.
			if (Parent == Root)
			{
				Nodes[Parent].bIsBlack = true;
				break;
			}
            else if (IsBlack(Parent))
            {
				break;
            }

            const FIntervalTreeNodeIndex GrandParent = Nodes[Parent].Parent;

            UE_ASSUME((Parent != GrandParent) && (Current != GrandParent));

			const bool bGrandParentIsRoot = GrandParent == Root;

            const bool bParentIsLeft = Nodes[GrandParent].Left == Parent;

            // The uncle is the other node of our parent.
            const FIntervalTreeNodeIndex Uncle = bParentIsLeft ? Nodes[GrandParent].Right : Nodes[GrandParent].Left;

            UE_ASSUME((GrandParent != Uncle) && (Parent != Uncle) && (Current != Uncle));

            if (!IsBlack(Uncle))
            {
				ASSERT(IsBlack(GrandParent));
                Nodes[Parent].bIsBlack = true;
                Nodes[Uncle].bIsBlack = true;

				if (bGrandParentIsRoot)
				{
					break;
				}
				else
				{
					Nodes[GrandParent].bIsBlack = false;
				}

                Current = GrandParent;
                continue;
            }

            // Our uncle is black, so we need to swizzle around.
            const bool CurrentIsLeft = Nodes[Parent].Left == Current;

            if (bParentIsLeft)
            {
                if (!CurrentIsLeft)
                {
                    Nodes[Parent].Right = Nodes[Current].Left;

					if (IntervalTreeNodeIndexNone != Nodes[Parent].Right)
					{
						Nodes[Nodes[Parent].Right].Parent = Parent;
					}

                    Nodes[Parent].Parent = Current;
                    Nodes[Current].Parent = GrandParent;
                    Nodes[Current].Left = Parent;
					Nodes[GrandParent].Left = Current;
                    std::swap(Parent, Current);
                }

                Nodes[Parent].Parent = Nodes[GrandParent].Parent;
                Nodes[GrandParent].Left = Nodes[Parent].Right;

				if (IntervalTreeNodeIndexNone != Nodes[GrandParent].Left)
				{
					Nodes[Nodes[GrandParent].Left].Parent = GrandParent;
				}

                Nodes[Parent].Right = GrandParent;
                Nodes[GrandParent].Parent = Parent;

                std::swap(Nodes[GrandParent].bIsBlack, Nodes[Parent].bIsBlack);
            }
            else
            {
                if (CurrentIsLeft)
                {
                    Nodes[Parent].Left = Nodes[Current].Right;
					
					if (IntervalTreeNodeIndexNone != Nodes[Parent].Left)
					{
						Nodes[Nodes[Parent].Left].Parent = Parent;
					}

                    Nodes[Parent].Parent = Current;
                    Nodes[Current].Parent = GrandParent;
                    Nodes[Current].Right = Parent;
					Nodes[GrandParent].Right = Current;
                    std::swap(Parent, Current);
                }

                Nodes[Parent].Parent = Nodes[GrandParent].Parent;
                Nodes[GrandParent].Right = Nodes[Parent].Left;

				if (IntervalTreeNodeIndexNone != Nodes[GrandParent].Right)
				{
					Nodes[Nodes[GrandParent].Right].Parent = GrandParent;
				}

                Nodes[Parent].Left = GrandParent;
				Nodes[GrandParent].Parent = Parent;

                std::swap(Nodes[GrandParent].bIsBlack, Nodes[Parent].bIsBlack);
            }

			if (bGrandParentIsRoot)
			{
				Root = Parent;
			}
			else
			{
				FIntervalTreeNode& GreatGrandParent = Nodes[Nodes[Parent].Parent];
				if (GrandParent == GreatGrandParent.Left)
				{
					GreatGrandParent.Left = Parent;
				}
				else
				{
					GreatGrandParent.Right = Parent;
				}
			}

            break;
        }

		if constexpr (bExtraDebugging)
		{
			for (FIntervalTreeNodeIndex Index = 0; Index < Nodes.Num(); Index++)
			{
				const FIntervalTreeNodeIndex Parent = Nodes[Index].Parent;
				if (IntervalTreeNodeIndexNone == Parent)
				{
					ASSERT(Root == Index);
				}
				else
				{
					ASSERT(Nodes[Parent].bIsBlack || Nodes[Index].bIsBlack);
					ASSERT((Nodes[Parent].Left == Index) ^ (Nodes[Parent].Right == Index));
				}
			}
		}

        return true;
    }

    FORCENOINLINE bool Contains(const void* const Address, const size_t Size) const
    {
        FRange NewRange(Address, Size);

        if (UNLIKELY(IntervalTreeNodeIndexNone == Root))
        {
            return false;
        }

        FIntervalTreeNodeIndex Current = Root;

        do
        {
            const FIntervalTreeNode& Node = Nodes[Current];

            const FRange Range = Node.Range;

            // This check does not need to prove that NewRange is entirely
            // enclosed within Range, because if any byte of NewRange was in the
            // original Range then it **must** already have been new memory.
            if ((NewRange.Start < Range.End) && (Range.Start < NewRange.End))
            {
                return true;
            }
            else if (NewRange.Start < Range.Start)
            {
                Current = Node.Left;
            }
            else
            {
                Current = Node.Right;
            }
        } while (IntervalTreeNodeIndexNone != Current);

        return false;
    }

    bool IsEmpty() const
    {
        return IntervalTreeNodeIndexNone == Root;
    }

    void Reset()
    {
        Root = IntervalTreeNodeIndexNone;
        Nodes.Empty();
    }

private:
    struct FRange final
    {
        FRange(const void* const Address, const size_t Size) :
            Start(reinterpret_cast<uintptr_t>(Address)), End(reinterpret_cast<uintptr_t>(Address) + Size) {}

        uintptr_t Start;
        uintptr_t End;
    };

private:
    using FIntervalTreeNodeIndex = uint32_t;

    static constexpr FIntervalTreeNodeIndex IntervalTreeNodeIndexNone = UINT32_MAX;

    struct FIntervalTreeNode final
    {
        explicit FIntervalTreeNode(const FRange Range) :
            Parent(IntervalTreeNodeIndexNone), bIsBlack(true), Range(Range) {}

		FIntervalTreeNode(const FRange Range, FIntervalTreeNodeIndex Parent) :
			Parent(Parent), bIsBlack(false), Range(Range) {}

        FIntervalTreeNodeIndex Left = IntervalTreeNodeIndexNone;
        FIntervalTreeNodeIndex Right = IntervalTreeNodeIndexNone;
        FIntervalTreeNodeIndex Parent;

        // TODO: optimize this waste of memory.
        bool bIsBlack;
        FRange Range;
    };

    FIntervalTreeNodeIndex Root = IntervalTreeNodeIndexNone;
    TPagedArray<FIntervalTreeNode> Nodes;

	static constexpr bool bExtraDebugging = false;
};

} // namespace AutoRTFM
