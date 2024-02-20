// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticLightingBuildContext.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/MapBuildDataRegistry.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR

FStaticLightingBuildContext::FStaticLightingBuildContext(UWorld* InWorld, ULevel* InLightingScenario) 	
{
	checkf(InWorld, TEXT("Constructing a FStaticLightingBuildContext requires a valid World"));

	World = InWorld;
	LightingScenario = InLightingScenario;

	if (LightingScenario)
	{
		MapBuildDataRegistry = InLightingScenario->MapBuildData;
	}
	else
	{
		MapBuildDataRegistry = InWorld->PersistentLevel->MapBuildData;
	}

	for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = World->GetLevel(LevelIndex);
		FGuid LevelGuid = FGuid(0, 0, 0, LevelIndex);
		LevelGuids.Add(LevelGuid, Level);
	}
	
	FGuid FirstGuid = FGuid(0, 0, 0, 0);
	check(GetLevelForGuid(FirstGuid) == World->PersistentLevel);
}

FStaticLightingBuildContext::FStaticLightingBuildContext(const FStaticLightingBuildContext& InFrom)
{
	MapBuildDataRegistry = InFrom.MapBuildDataRegistry;
	LevelGuids = InFrom.LevelGuids;
	World = InFrom.World;
	LightingScenario = InFrom.LightingScenario;
}

bool FStaticLightingBuildContext::ShouldIncludeActor(AActor* Actor) const
{
	check(Actor);

	ULevel* ActorLevel = Actor->GetLevel();
	check(ActorLevel);

	bool IncludeActor = false;

	if (!LightingScenario || !ActorLevel->bIsLightingScenario || ActorLevel == LightingScenario)
	{
		IncludeActor = true;
	}

	return IncludeActor;
}

bool FStaticLightingBuildContext::ShouldIncludeLevel(ULevel* Level) const
{
	check(Level);

	bool IncludeLevel = false;

	if (!LightingScenario || !Level->bIsLightingScenario || Level == LightingScenario)
	{
		IncludeLevel = true;
	}

	return IncludeLevel;
}

UMapBuildDataRegistry* FStaticLightingBuildContext::GetGlobalRegistry() const
{
	return MapBuildDataRegistry;
}

UMapBuildDataRegistry* FStaticLightingBuildContext::GetOrCreateGlobalRegistry() const
{
	if (!MapBuildDataRegistry)
	{
		if (LightingScenario)
		{
			MapBuildDataRegistry = LightingScenario->GetOrCreateMapBuildData();
		}
		else
		{
			MapBuildDataRegistry = World->PersistentLevel->GetOrCreateMapBuildData();
		}
	}
	
	return MapBuildDataRegistry;
}

ULevel* FStaticLightingBuildContext::GetLightingStorageLevel(ULevel* Level) const
{
	if (LightingScenario)
	{
		return LightingScenario;
	}
	else
	{
		return Level;
	}
}

UMapBuildDataRegistry* FStaticLightingBuildContext::GetRegistryForActor(AActor* Actor) const
{	
	check(Actor);

	ULevel* Level = Actor->GetLevel();

	return GetLightingStorageLevel(Level)->MapBuildData;	
}

FGuid FStaticLightingBuildContext::GetPersistentLevelGuid() const
{
	return *LevelGuids.FindKey(World->PersistentLevel);
}

FGuid FStaticLightingBuildContext::GetLevelGuidForLevel(ULevel* Level) const
{
	return *LevelGuids.FindKey(Level);
}

FGuid FStaticLightingBuildContext::GetLevelGuidForActor(AActor* Actor) const
{		
	check(Actor);
		
	if (!World->IsPartitionedWorld())
	{
		return *LevelGuids.FindKey(Actor->GetLevel());
	}

	FGuid LevelGuid = FGuid(0, 0, 0, 0);		
	return LevelGuid;
}


UMapBuildDataRegistry* FStaticLightingBuildContext::GetOrCreateRegistryForLevelGuid(const FGuid& Guid) const
{	
	if (Guid.IsValid())
	{	
		if (!World->IsPartitionedWorld())
		{
			ULevel* Level = GetLevelForGuid(Guid).Get();
			return Level->GetOrCreateMapBuildData();
		}
		else
		{
		}
	}

	UMapBuildDataRegistry* Registry = GetOrCreateGlobalRegistry();
	return Registry;
}

const TWeakObjectPtr<ULevel> FStaticLightingBuildContext::GetLevelForGuid(const FGuid& Guid) const
{	
	return LevelGuids.FindRef(Guid);
}

FGuid FStaticLightingBuildContext::GetLevelBuildDataID(const FGuid& LevelGuid) const
{
	if (!World->IsPartitionedWorld())
	{
		return GetLevelForGuid(LevelGuid)->LevelBuildDataId;
	}
	else
	{
		if (!LevelGuid.IsValid())
		{
			return World->PersistentLevel->LevelBuildDataId;
		}
		
		return LevelGuid;			
	}
}

UMapBuildDataRegistry* FStaticLightingBuildContext::GetOrCreateRegistryForActor(AActor* Actor) const
{
	check(Actor);

	ULevel* Level = Actor->GetLevel();	

	return GetLightingStorageLevel(Level)->GetOrCreateMapBuildData();
}

UMapBuildDataRegistry* FStaticLightingBuildContext::GetRegistryForLevel(ULevel* Level) const
{
	return GetLightingStorageLevel(Level)->MapBuildData;
}

UMapBuildDataRegistry* FStaticLightingBuildContext::GetOrCreateRegistryForLevel(ULevel* Level) const
{
	return GetLightingStorageLevel(Level)->GetOrCreateMapBuildData();
}

#endif