// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "UObject/NameTypes.h"
#include "AvalancheVersionInfo.generated.h"

class FArchive;
struct FCustomVersion;

/** Holds file version information */
USTRUCT()
struct FAvalancheFileVersionInfo
{
	GENERATED_BODY()

	FAvalancheFileVersionInfo()
		: FileVersionUE4(0)
		, FileVersionUE5(0)
		, FileVersionLicensee(0)
	{
	}
	
	void UpdateToLatest();

	UPROPERTY()
	int32 FileVersionUE4;

	UPROPERTY()
	int32 FileVersionUE5;
	
	/* Licensee file version */
	UPROPERTY()
	int32 FileVersionLicensee;
};

/** Holds engine version information */
USTRUCT()
struct FAvalancheEngineVersionInfo
{
	GENERATED_BODY()

	FAvalancheEngineVersionInfo()
		: Major(0)
		, Minor(0)
		, Patch(0)
		, Changelist(0)
	{
	}
	
	void UpdateToLatest();

	/** Major version number */
	UPROPERTY()
	uint16 Major;

	/** Minor version number */
	UPROPERTY()
	uint16 Minor;

	/** Patch version number */
	UPROPERTY()
	uint16 Patch;

	/** Changelist number. This is used to arbitrate when Major/Minor/Patch version numbers match */
	UPROPERTY()
	uint32 Changelist;
};

/** Holds custom version information */
USTRUCT()
struct FAvalancheCustomVersionInfo
{
	GENERATED_BODY()

	FAvalancheCustomVersionInfo()
		: Version(0)
	{
	}
	
	void SetCustomVersion(const FCustomVersion& InVersion);
	
	/** Unique custom key */
	UPROPERTY()
	FGuid Key;

	/** Custom version */
	UPROPERTY()
	int32 Version;

	/** Friendly name of the version */
	UPROPERTY()
	FName FriendlyName;
};

/** Holds version information for a session */
USTRUCT()
struct FAvalancheVersionInfo
{
	GENERATED_BODY()

	/** Update all Versions (File, Engine, Custom) to Latest */
	void UpdateToLatest();

	void ApplyToArchive(FArchive& Archive) const;

	/** File version info */
	UPROPERTY()
	FAvalancheFileVersionInfo FileVersion;

	/** Engine version info */
	UPROPERTY()
	FAvalancheEngineVersionInfo EngineVersion;

	/** Custom version info */
	UPROPERTY()
	TArray<FAvalancheCustomVersionInfo> CustomVersions;

	/** Keep track of the custom versions used by the assets while saving. */
	TSet<FGuid> UsedCustomVersions;
};