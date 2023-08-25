// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorLoadedActorCache.h"

#if WITH_EDITOR

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"

FEditorLoadedActorCache::FEditorLoadedActorCache()
{
	// Get all Actors except CDOs including invalid ones since this is a loaded cache
	ForEachObjectOfClass(AActor::StaticClass(), [this](UObject* InObject)
	{
		AActor* Actor = Cast<AActor>(InObject);
		if (ULevel* Level = Actor->GetLevel())
		{
			LoadedActorsPerLevel.FindOrAdd(Level).Add(Actor->GetActorGuid());
		}
	}, true, RF_ClassDefaultObject, EInternalObjectFlags::None);
}

const TSet<FGuid>& FEditorLoadedActorCache::GetLoadedActorsForLevel(ULevel* InLevel) const
{
	if (const TSet<FGuid>* LoadedActors = LoadedActorsPerLevel.Find(InLevel))
	{
		return *LoadedActors;
	}

	static TSet<FGuid> EmptyGuids;
	return EmptyGuids;
}

#endif