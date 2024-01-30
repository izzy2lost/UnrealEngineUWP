// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheDataDefines.generated.h"

USTRUCT()
struct FAvaObjectIndex
{
	GENERATED_BODY()

	FAvaObjectIndex() {}

	FAvaObjectIndex(int32 Index) : Index(Index) {}

	bool operator==(const FAvaObjectIndex& Other) const
	{
		return Index == Other.Index;
	}
	
	friend uint32 GetTypeHash(const FAvaObjectIndex& Value)
    {
    	return GetTypeHash(Value.Index);
    }

	UPROPERTY()
	int32 Index = INDEX_NONE;
};
