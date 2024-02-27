// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemAllocInSwapNode.h"

#define LOCTEXT_NAMESPACE "Insights::FMemAllocSwapNode"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FMemAllocInSwapNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 FMemAllocInSwapNode::GetBytesInSwapPage() const
{
	return BytesInSwapPage;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
