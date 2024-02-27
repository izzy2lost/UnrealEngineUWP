// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Insights/MemoryProfiler/ViewModels/MemAllocNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Class used to store information about an allocation node in a particular swap page.
*/
class FMemAllocInSwapNode : public FMemAllocNode
{
	INSIGHTS_DECLARE_RTTI(FMemAllocInSwapNode, FMemAllocNode)

public:
	/** Initialization constructor for the MemAlloc node. */
	explicit FMemAllocInSwapNode(const FName InName, TWeakPtr<FMemAllocTable> InParentTable, int32 InRowIndex, uint64 InBytesInSwapPage)
		: FMemAllocNode(InName, InParentTable, InRowIndex)
		, BytesInSwapPage(InBytesInSwapPage)
	{
	}

	uint64 GetBytesInSwapPage() const;

private:
	const uint64 BytesInSwapPage;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
