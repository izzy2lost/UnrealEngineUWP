// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Elements/Common/TypedElementCommonTypes.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Containers/VersePath.h"

#include "TedsAssetDataColumns.generated.h"

USTRUCT()
struct FAssetPathColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName Path;
};

USTRUCT()
struct FParentAssetPathColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	TypedElementDataStorage::RowHandle ParentRow;
};

USTRUCT()
struct FChildrenAssetPathColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	TSet<TypedElementDataStorage::RowHandle> ChildrenRows;
};

USTRUCT()
struct FAssetsInPathColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	TSet<TypedElementDataStorage::RowHandle> AssetsRow;
};

USTRUCT()
struct FAssetDataColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	FAssetData AssetData;
};

USTRUCT()
struct FUnresolvedParentAssetPathColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	TypedElementDataStorage::IndexHash Hash;
};

USTRUCT()
struct FUnresolvedAssetsInPathColumn_Experimental : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	TypedElementDataStorage::IndexHash Hash;
};

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