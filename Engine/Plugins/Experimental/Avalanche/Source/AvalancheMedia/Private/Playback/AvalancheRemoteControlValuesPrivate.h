// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "AvalancheRemoteControlValuesPrivate.generated.h"

/** Custom serialization version for FAvalancheRemoteControlValue */
struct FAvalancheRemoteControlValueCustomVersion
{
	enum Type
	{
		// Initial version had the values stored as raw bytes.
		BeforeCustomVersionWasAdded = 0,

		// Values are stored as strings (json formatted).
		ValueAsString,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	static const FGuid Key;
	
private:
	FAvalancheRemoteControlValueCustomVersion() = delete;
};

USTRUCT()
struct FAvalancheRemoteControlValueAsBytes_Legacy
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<uint8> Bytes;

	UPROPERTY()
	bool bIsDefault = false;
};