// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Containers/VersePath.h"

#include "TypedElementAssetColumns.generated.h"

// Tag to identify assets
USTRUCT(meta = (DisplayName = "Asset"))
struct FAssetTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

// Tag to identify assets with private visibility
USTRUCT(meta = (DisplayName = "Private Asset"))
struct FPrivateAssetTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

// Tag to identify assets with public visibility
USTRUCT(meta = (DisplayName = "Public Asset"))
struct FPublicAssetTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

// Column to store the disk size of an asset
USTRUCT(meta = (DisplayName = "Disk Size"))
struct FDiskSizeColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	// Size on disk (in bytes)
	UPROPERTY()
	int64 DiskSize;
};

// Column to store the verse path of an asset
USTRUCT(meta = (DisplayName = "Verse Path"))
struct FVersePathColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UE::Core::FVersePath VersePath;
};