// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreFwd.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/Guid.h"

class ULevel;
class UWorld;

/**
 * This class should be used as a snapshot cache of loaded actors. 
 * It doesn't update its internal state after creation.
 */
struct ENGINE_API FEditorLoadedActorCache
{
public:
	FEditorLoadedActorCache();
		
	const TSet<FGuid>& GetLoadedActorsForLevel(ULevel* InLevel) const;

private:
	TMap<ULevel*, TSet<FGuid>> LoadedActorsPerLevel;
};

#endif