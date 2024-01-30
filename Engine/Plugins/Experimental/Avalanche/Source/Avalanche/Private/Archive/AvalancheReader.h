// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheArchive.h"

class FAvalancheReader : public FAvalancheArchive
{
	using Super = FAvalancheArchive;
	
public:
	
	FAvalancheReader(FAvalancheWorldData& InWorldData, FAvalancheObjectData& InObjectData, UObject* InObject);

	// FAvalancheArchive Interface
	virtual UObject* ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const override;
	// ~FAvalancheArchive Interface
};
