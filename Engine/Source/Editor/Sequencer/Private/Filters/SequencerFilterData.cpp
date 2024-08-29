// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/SequencerFilterData.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModels/ViewModelIterators.h"

using namespace UE::Sequencer;

FSequencerFilterData::FSequencerFilterData(const FString& InRawFilterText)
	: RawFilterText(InRawFilterText)
{
}

bool FSequencerFilterData::operator==(const FSequencerFilterData& InRhs) const
{
	return ContainsFilterInNodes(InRhs) && TotalNodeCount == InRhs.GetTotalNodeCount();
}

bool FSequencerFilterData::operator!=(const FSequencerFilterData& InRhs) const
{
	return !(*this == InRhs);
}

void FSequencerFilterData::Reset()
{
	FilterInNodes.Reset();

	TotalNodeCount = 0;
}

FString FSequencerFilterData::GetRawFilterText() const
{
	return RawFilterText;
}

uint32 FSequencerFilterData::GetDisplayNodeCount() const
{
	return FilterInNodes.Num();
}

uint32 FSequencerFilterData::GetTotalNodeCount() const
{
	return TotalNodeCount;
}

uint32 FSequencerFilterData::GetFilterInCount() const
{
	return FilterInNodes.Num();
}

uint32 FSequencerFilterData::GetFilterOutCount() const
{
	return GetTotalNodeCount() - GetFilterInCount();
}

void FSequencerFilterData::IncrementTotalNodeCount()
{
	++TotalNodeCount;
}

void FSequencerFilterData::FilterInNode(TWeakViewModelPtr<IOutlinerExtension> InNodeWeak)
{
	FilterInNodes.Add(InNodeWeak);

	if (const TViewModelPtr<IOutlinerExtension> Node = InNodeWeak.Pin())
	{
		Node->SetFilteredOut(false);
	}
}

void FSequencerFilterData::FilterOutNode(TWeakViewModelPtr<IOutlinerExtension> InNodeWeak)
{
	const FSetElementId ElementId = FilterInNodes.FindId(InNodeWeak);
	if (ElementId.IsValidId())
	{
		FilterInNodes.Remove(ElementId);
	}

	if (const TViewModelPtr<IOutlinerExtension> Node = InNodeWeak.Pin())
	{
		Node->SetFilteredOut(true);
	}
}

void FSequencerFilterData::FilterInParentChildNodes(const TViewModelPtr<IOutlinerExtension>& InNode
	, const bool bInIncludeSelf, const bool bInIncludeParents, const bool bInIncludeChildren)
{
	if (!InNode.IsValid())
	{
		return;
	}

	if (bInIncludeParents)
	{
		for (TViewModelPtr<IOutlinerExtension> ParentNode : InNode.AsModel()->GetAncestorsOfType<IOutlinerExtension>())
		{
			FilterInNode(ParentNode);
		}
	}

	if (bInIncludeSelf)
	{
		FilterInNode(InNode);
	}

	if (bInIncludeChildren)
	{
		for (TViewModelPtr<IOutlinerExtension> ChildNode : InNode.AsModel()->GetDescendantsOfType<IOutlinerExtension>())
		{
			FilterInNode(ChildNode);
		}
	}
}

void FSequencerFilterData::FilterInNodeWithAncestors(const TViewModelPtr<IOutlinerExtension>& InNode)
{
	FilterInParentChildNodes(InNode, true, true, false);
}

bool FSequencerFilterData::ContainsFilterInNodes(const FSequencerFilterData& InOtherData) const
{
	return FilterInNodes.Includes(InOtherData.FilterInNodes);
}

bool FSequencerFilterData::IsFilteredOut(const TViewModelPtr<IOutlinerExtension>& InNode) const
{
	return !FilterInNodes.Contains(InNode);
}
