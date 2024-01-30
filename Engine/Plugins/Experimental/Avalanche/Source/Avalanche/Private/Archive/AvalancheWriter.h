// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheArchive.h"
#include "Misc/AssertionMacros.h"

class FAvalancheWriter : public FAvalancheArchive
{
	using Super = FAvalancheArchive;

public:
	
	FAvalancheWriter(FAvalancheWorldData& InWorldData, FAvalancheObjectData& InObjectData, UObject* InObject);

	// FAvalancheArchive Interface
	virtual UObject* ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const override
	{
		checkNoEntry();
		return nullptr;
	}
	// ~FAvalancheArchive Interface
};
