// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshDrawCommandStatsComponentData.h"

#if MESH_DRAW_COMMAND_STAT_COLLECTION

FMeshDrawCommandStatsComponentDataManager* FMeshDrawCommandStatsComponentDataManager::Instance = nullptr;

void FMeshDrawCommandStatsComponentDataManager::CreateInstance()
{
	check(Instance == nullptr);
	Instance = new FMeshDrawCommandStatsComponentDataManager();
}

FMeshDrawCommandStatsComponentDataID FMeshDrawCommandStatsComponentDataManager::GetID(const FMeshDrawCommandStatsComponentData& ComponentData)
{
	FRWScopeLock WriteLock(RWLock, SLT_Write);

	FMeshDrawCommandStatsComponentDataID* ID = ComponentDataLookupMap.Find(ComponentData);
	if (ID)
	{
		return *ID;
	}

	// Make sure it still fits in 16 bits, otherwise ID type needs to be changed to uint32
	check(CachedComponentData.Num() < MAX_uint16);

	FMeshDrawCommandStatsComponentDataID NewID = CachedComponentData.Num();
	CachedComponentData.Add(ComponentData);
	ComponentDataLookupMap.Add(ComponentData, NewID);
	return NewID;
}

#endif // MESH_DRAW_COMMAND_STAT_COLLECTION