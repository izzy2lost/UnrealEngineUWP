// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GarbageCollectionGlobals.h: Garbage Collection Global State Vars
=============================================================================*/

#pragma once

#include "UObject/ObjectMacros.h"

namespace UE::GC
{
	/** Current EInternalObjectFlags value representing an unreachable object */
	extern COREUOBJECT_API EInternalObjectFlags GUnreachableObjectFlag;

	/** Current EInternalObjectFlags value representing a maybe unreachable object */
	extern COREUOBJECT_API EInternalObjectFlags GMaybeUnreachableObjectFlag;
}
