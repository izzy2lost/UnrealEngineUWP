// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryAlloc.h"

#include "Insights/MemoryProfiler/ViewModels/CallstackFormatting.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemoryAlloc::FMemoryAlloc()
	: StartEventIndex(0)
	, EndEventIndex(0)
	, StartTime(0.0)
	, EndTime(0.0)
	, Address(0)
	, Size(0)
	, TagId(0)
	, Tag(nullptr)
	, Asset(nullptr)
	, Package(nullptr)
	, Callstack(nullptr)
	, FreeCallstack(nullptr)
	, RootHeap(0)
	, bIsHeap(false)
	, bIsDecline(false)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemoryAlloc::~FMemoryAlloc()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
