// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParamContextData.generated.h"

namespace UE::AnimNext
{
	struct FParamStackLayerHandle;
}

USTRUCT()
struct FAnimNextParamContextData
{
	GENERATED_BODY()

	FAnimNextParamContextData() = default;

	FAnimNextParamContextData(UE::AnimNext::FParamStackLayerHandle& InLayerHandle)
		: LayerHandle(&InLayerHandle)
	{}

	UE::AnimNext::FParamStackLayerHandle& GetLayerHandle() const
	{
		return *LayerHandle;
	}

private:
	// Call this to reset the context to its original state to detect stale usage
	void Reset()
	{
		LayerHandle = nullptr;
	}

	// Parameter scope context
	UE::AnimNext::FParamStackLayerHandle* LayerHandle = nullptr;

	friend class UAnimNextModule;
	friend struct FAnimNextExecuteContext;
};
