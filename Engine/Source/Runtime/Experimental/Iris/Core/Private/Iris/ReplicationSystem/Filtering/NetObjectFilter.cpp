// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"

UNetObjectFilter::UNetObjectFilter()
{
}

void UNetObjectFilter::Init(const FNetObjectFilterInitParams& Params)
{
	FilteredObjects.Init(Params.MaxObjectCount);

	{
		UE::Net::Private::FNetObjectFilteringInfoAccessor FilteringInfoAccessor;
		FilteringInfos = FilteringInfoAccessor.GetNetObjectFilteringInfos(Params.ReplicationSystem);
	}

	OnInit(Params);
}

void UNetObjectFilter::AddConnection(uint32 ConnectionId)
{
}

void UNetObjectFilter::RemoveConnection(uint32 ConnectionId)
{
}

void UNetObjectFilter::UpdateObjects(FNetObjectFilterUpdateParams&)
{
}

void UNetObjectFilter::PreFilter(FNetObjectPreFilteringParams&)
{
}

void UNetObjectFilter::Filter(FNetObjectFilteringParams&)
{
}

void UNetObjectFilter::PostFilter(FNetObjectPostFilteringParams&)
{
}

FNetObjectFilteringInfo* UNetObjectFilter::GetFilteringInfo(uint32 ObjectIndex)
{
	// Only allow retreiving infos for objects handled by this instance.
	if (!IsObjectFiltered(ObjectIndex))
	{
		return nullptr;
	}

	return &FilteringInfos[ObjectIndex];
}

