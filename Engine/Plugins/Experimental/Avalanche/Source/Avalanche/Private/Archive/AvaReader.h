// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArchive.h"

class FAvaReader : public FAvaArchive
{
	using Super = FAvaArchive;
	
public:
	
	FAvaReader(FAvaWorldData& InWorldData, FAvaObjectData& InObjectData, UObject* InObject);

	// FAvaArchive Interface
	virtual UObject* ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const override;
	// ~FAvaArchive Interface
};
