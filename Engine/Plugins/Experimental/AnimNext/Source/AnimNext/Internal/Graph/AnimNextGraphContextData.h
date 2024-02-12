// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextGraphContextData.generated.h"

struct FAnimNextGraphInstance;

namespace UE::AnimNext
{
	struct FLatentPropertyHandle;
}

USTRUCT()
struct FAnimNextGraphContextData
{
	GENERATED_BODY()

	FAnimNextGraphContextData() = default;

	FAnimNextGraphContextData(const FAnimNextGraphInstance* InInstance, const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& InLatentHandles, void* InDestinationBasePtr, bool bInIsFrozen)
		: Instance(InInstance)
		, LatentHandles(InLatentHandles)
		, DestinationBasePtr(InDestinationBasePtr)
		, bIsFrozen(bInIsFrozen)
	{
	}

	const FAnimNextGraphInstance& GetGraphInstance() const { check(Instance); return *Instance; }
	const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& GetLatentHandles() const { return LatentHandles; }
	void* GetDestinationBasePtr() const { return DestinationBasePtr; }
	bool IsFrozen() const { return bIsFrozen; }

private:
	// Call this to reset the context to its original state to detect stale usage
	void Reset()
	{
		Instance = nullptr;
		LatentHandles = TConstArrayView<UE::AnimNext::FLatentPropertyHandle>();
		DestinationBasePtr = nullptr;
		bIsFrozen = false;
	}

	const FAnimNextGraphInstance* Instance = nullptr;
	TConstArrayView<UE::AnimNext::FLatentPropertyHandle> LatentHandles;
	void* DestinationBasePtr = nullptr;
	bool bIsFrozen = false;

	friend struct FAnimNextGraphInstance;
	friend struct FAnimNextExecuteContext;
};
