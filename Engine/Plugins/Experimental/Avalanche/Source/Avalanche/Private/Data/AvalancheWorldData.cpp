// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/AvalancheWorldData.h"

void FAvalancheWorldData::FinalizeSave()
{
	VersionInfo.UpdateToLatest();
}

void FAvalancheWorldData::ResetTransientData()
{
	World.Reset();
	NameIndexMap.Reset();
	ObjectReferenceIndexMap.Reset();
	CachedActors.Reset();
	CachedSubObjects.Reset();
}
