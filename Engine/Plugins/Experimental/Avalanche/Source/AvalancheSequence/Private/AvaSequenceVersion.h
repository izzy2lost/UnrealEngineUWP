// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"

struct FAvaSequenceVersion
{
private:
	FAvaSequenceVersion() = delete;

public:
	enum Type : uint8
	{
		PreVersioning = 0,

		/** Moved to become child class of ULevelSequence */
		LevelSequence, 

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	const static FGuid GUID;
};
