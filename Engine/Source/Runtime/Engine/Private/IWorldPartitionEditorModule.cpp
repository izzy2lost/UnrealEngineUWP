// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "Modules/ModuleManager.h"

IWorldPartitionEditorModule* IWorldPartitionEditorModule::Instance = nullptr;

IWorldPartitionEditorModule& IWorldPartitionEditorModule::Get()
{
	if (!Instance)
	{
		Instance = &FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");
	}
	return *Instance;
}