// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"

struct FSoftObjectPath;

namespace UE::ConcertClientSharedSlate::ObjectUtils
{
	/** @return Checks whether the given object is an actor. */
	bool IsActor(const FSoftObjectPath& Object);
	
	/** @return Get the owning actor of Subobject. If Subobject is an actor, then this returns unset. */
	TOptional<FSoftObjectPath> GetActorOf(const FSoftObjectPath& Subobject);
};
