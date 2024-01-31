// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/AvaWorldData.h"

void FAvaWorldData::FinalizeSave()
{
	VersionInfo.UpdateToLatest();
}

void FAvaWorldData::ResetTransientData()
{
	World.Reset();
	NameIndexMap.Reset();
	ObjectReferenceIndexMap.Reset();
	CachedActors.Reset();
	CachedSubObjects.Reset();
}
