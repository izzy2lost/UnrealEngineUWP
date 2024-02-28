// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsSettings.h"
#include "ClientInstancedActorsSpawnerSubsystem.h"
#include "ServerInstancedActorsSpawnerSubsystem.h"
#include "InstancedActorsSubsystem.h"
#include "InstancedActorsVisualizationTrait.h"


//-----------------------------------------------------------------------------
// UInstancedActorsProjectSettings
//-----------------------------------------------------------------------------
UInstancedActorsProjectSettings::UInstancedActorsProjectSettings()
{
	ServerActorSpawnerSubsystemClass = UServerInstancedActorsSpawnerSubsystem::StaticClass();
	ClientActorSpawnerSubsystemClass = UClientInstancedActorsSpawnerSubsystem::StaticClass();
	InstancedActorsSubsystemClass = UInstancedActorsSubsystem::StaticClass();
	StationaryVisualizationTraitClass = UInstancedActorsVisualizationTrait::StaticClass();
}

TSubclassOf<UMassActorSpawnerSubsystem> UInstancedActorsProjectSettings::GetServerActorSpawnerSubsystemClass() const 
{ 
	return ServerActorSpawnerSubsystemClass.TryLoadClass<UMassActorSpawnerSubsystem>(); 
}

TSubclassOf<UMassActorSpawnerSubsystem> UInstancedActorsProjectSettings::GetClientActorSpawnerSubsystemClass() const 
{ 
	return ClientActorSpawnerSubsystemClass.TryLoadClass<UMassActorSpawnerSubsystem>(); 
}

TSubclassOf<UInstancedActorsSubsystem> UInstancedActorsProjectSettings::GetInstancedActorsSubsystemClass() const 
{ 
	return InstancedActorsSubsystemClass.TryLoadClass<UInstancedActorsSubsystem>(); 
}

TSubclassOf<UMassStationaryDistanceVisualizationTrait> UInstancedActorsProjectSettings::GetStationaryVisualizationTraitClass() const
{
	return StationaryVisualizationTraitClass.TryLoadClass<UMassStationaryDistanceVisualizationTrait>();
}
