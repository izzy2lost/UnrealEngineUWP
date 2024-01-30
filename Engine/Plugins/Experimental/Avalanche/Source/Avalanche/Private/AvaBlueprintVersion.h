// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"

/** Version used for serializing Avalanche Blueprint Data */
struct FAvalancheBlueprintVersion
{
private:
	FAvalancheBlueprintVersion() = delete;

public:
	enum Type : uint8
	{
		PreVersioning = 0, // From before file versioning was implemented

		//new versions can be added above here
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	const static FGuid GUID;
};
