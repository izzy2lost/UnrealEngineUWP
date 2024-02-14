// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreFwd.h"

class FObjectArchetypeHelper
{
public:
	class IObjectArchetypePolicy
	{
#if WITH_EDITOR
	public:
		virtual UObject* GetArchetype(const UObject* InObject) const = 0;
#endif
	};

#if WITH_EDITOR
private:
	// Restrict usage to FPropertyNode
	friend class FPropertyNode;
#endif
	COREUOBJECT_API static UObject* GetArchetype(const UObject* InObject, const IObjectArchetypePolicy* InPolicy);
};
