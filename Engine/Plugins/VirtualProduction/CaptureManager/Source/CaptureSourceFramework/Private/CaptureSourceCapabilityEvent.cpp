// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceCapabilityEvent.h"

FEventDesc::FEventDesc(FString InName, TArray<FPropertyDesc> InParams)
	: Name(MoveTemp(InName))
	, Params(MoveTemp(InParams))
{
}

void FEventList::AddEvent(FEventDesc InEventDesc)
{
	Events.Add(MoveTemp(InEventDesc));
}

TArray<FEventDesc> FEventList::GetEvents() const
{
	return Events;
}

void FEventList::CheckEventExists(const FString& InName) const
{
	check(Events.ContainsByPredicate([&InName](const FEventDesc& InEvent)
	{
		return InEvent.Name == InName;
	}));
}