// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsRepresentationSubsystem.h"
#include "InstancedActorsSettings.h"


void UInstancedActorsRepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TSubclassOf<UMassActorSpawnerSubsystem> SpawnerSystemSubclass;
	// @todo Add support for non-replay NM_Standalone where we should use UServerInstancedActorsSpawnerSubsystem for 
	// authoritative actor spawning.
	if (GetWorldRef().GetNetMode() == NM_DedicatedServer)
	{
		SpawnerSystemSubclass = GET_INSTANCEDACTORS_CONFIG_VALUE(GetServerActorSpawnerSubsystemClass());

	}
	else
	{
		SpawnerSystemSubclass = GET_INSTANCEDACTORS_CONFIG_VALUE(GetClientActorSpawnerSubsystemClass());
	}

	if (ensure(SpawnerSystemSubclass))
	{
		ActorSpawnerSubsystem = Cast<UMassActorSpawnerSubsystem>(Collection.InitializeDependency(SpawnerSystemSubclass));

		ensureMsgf(ActorSpawnerSubsystem, TEXT("Trying to initialize dependency on class %s failed. Verify InstanedActors settings.")
			, *GetNameSafe(ActorSpawnerSubsystem));
	}
}
