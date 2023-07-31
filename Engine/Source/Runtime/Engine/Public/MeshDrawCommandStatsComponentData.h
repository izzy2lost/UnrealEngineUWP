// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/NameTypes.h"

#define MESH_DRAW_COMMAND_STAT_COLLECTION !UE_BUILD_SHIPPING

#if MESH_DRAW_COMMAND_STAT_COLLECTION

/**
 * Extra data stored per UPrimitiveComponent which is used during MeshDrawCommand stat collection
 */
struct FMeshDrawCommandStatsComponentData
{
	bool operator==(const FMeshDrawCommandStatsComponentData& Other) const
	{
		return ComponentType == Other.ComponentType && StatsCategory == Other.StatsCategory;
	}

	/** Type name of the component. */
	FName ComponentType;
	/** Category used to aggregate mesh draw stats. */
	FName StatsCategory;
};

static uint32 GetTypeHash(const FMeshDrawCommandStatsComponentData& StatsInfo)
{
	return HashCombine(GetTypeHash(StatsInfo.ComponentType), GetTypeHash(StatsInfo.StatsCategory));
}

using FMeshDrawCommandStatsComponentDataID = int16;

/**
 * Manages all created FMeshDrawCommandStatsComponentData and exposed unique FMeshDrawCommandStatsComponentDataID to retrieve the data again during stat processing
 */
class ENGINE_API FMeshDrawCommandStatsComponentDataManager
{
public:
	static void CreateInstance();
	static FMeshDrawCommandStatsComponentDataManager* Get() { return Instance; }

	FMeshDrawCommandStatsComponentDataID GetID(const FMeshDrawCommandStatsComponentData& ComponentData);
	FMeshDrawCommandStatsComponentData GetComponentData(FMeshDrawCommandStatsComponentDataID ID) const
	{
		if (ID == INDEX_NONE)
		{
			return FMeshDrawCommandStatsComponentData();
		}

		FRWScopeLock ReadLock(RWLock, SLT_ReadOnly);
		return CachedComponentData[ID];
	}

private:	
	mutable FRWLock RWLock;
	TArray<FMeshDrawCommandStatsComponentData> CachedComponentData;
	TMap<FMeshDrawCommandStatsComponentData, FMeshDrawCommandStatsComponentDataID> ComponentDataLookupMap;

	static FMeshDrawCommandStatsComponentDataManager* Instance;
};

#endif // MESH_DRAW_COMMAND_STAT_COLLECTION

