// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionRuntimeCellData.h"
#include "WorldPartitionRuntimeCellDataHashSet.generated.h"

UCLASS(Within = WorldPartitionRuntimeCell, MinimalAPI)
class UWorldPartitionRuntimeCellDataHashSet : public UWorldPartitionRuntimeCellData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bIs2D;
};
