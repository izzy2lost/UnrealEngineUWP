// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/EventSourceUtils.h"

#include "CaptureSourceCapabilityProperty.h"

struct CAPTURESOURCEFRAMEWORK_API FEventDesc
{
	FEventDesc(FString InName, TArray<FPropertyDesc> InParams);

	FString Name;
	TArray<FPropertyDesc> Params;
};

class FEventList
{
public:

	FEventList() = default;

	void AddEvent(FEventDesc InEventDesc);
	TArray<FEventDesc> GetEvents() const;

	void CheckEventExists(const FString& InName) const;

private:

	TArray<FEventDesc> Events;
};
