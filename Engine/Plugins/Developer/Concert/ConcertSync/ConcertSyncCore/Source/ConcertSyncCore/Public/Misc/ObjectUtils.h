// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"

struct FSoftObjectPath;

namespace UE::ConcertSyncCore
{
	/** @return Checks whether the given object is an actor. */
	CONCERTSYNCCORE_API bool IsActor(const FSoftObjectPath& Object);
	
	/** @return Get the owning actor of Subobject. If Subobject is an actor, then this returns unset. */
	CONCERTSYNCCORE_API TOptional<FSoftObjectPath> GetActorOf(const FSoftObjectPath& Subobject);
	
	/** @return Gets the last object name in the subpath. */
	CONCERTSYNCCORE_API FString ExtractObjectNameFromPath(const FSoftObjectPath& Object);
};
