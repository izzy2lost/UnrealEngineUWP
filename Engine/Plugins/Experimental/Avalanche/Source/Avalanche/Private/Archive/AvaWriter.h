// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArchive.h"
#include "Misc/AssertionMacros.h"

class FAvaWriter : public FAvaArchive
{
	using Super = FAvaArchive;

public:
	
	FAvaWriter(FAvaWorldData& InWorldData, FAvaObjectData& InObjectData, UObject* InObject);

	// FAvaArchive Interface
	virtual UObject* ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const override
	{
		checkNoEntry();
		return nullptr;
	}
	// ~FAvaArchive Interface
};
