// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Elements/Common/TypedElementCommonTypes.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Containers/VersePath.h"

#include "TedsAssetDataColumns.generated.h"

USTRUCT()
struct FAssetPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName Path;
};

USTRUCT()
struct FParentAssetPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	UE::Editor::DataStorage::RowHandle ParentRow;
};

USTRUCT()
struct FChildrenAssetPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	TSet<UE::Editor::DataStorage::RowHandle> ChildrenRows;
};

USTRUCT()
struct FAssetsInPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	TSet<UE::Editor::DataStorage::RowHandle> AssetsRow;
};

USTRUCT()
struct FAssetDataColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	FAssetData AssetData;
};

USTRUCT()
struct FUnresolvedParentAssetPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	TypedElementDataStorage::IndexHash Hash;
};

USTRUCT()
struct FUnresolvedAssetsInPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	TypedElementDataStorage::IndexHash Hash;
};

// Tag to identify assets
USTRUCT(meta = (DisplayName = "Asset"))
struct FAssetTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

// Tag to identify assets with private visibility
USTRUCT(meta = (DisplayName = "Private Asset"))
struct FPrivateAssetTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

// Tag to identify assets with public visibility
USTRUCT(meta = (DisplayName = "Public Asset"))
struct FPublicAssetTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

// Column to store the type of an asset
USTRUCT(meta = (DisplayName = "Type Path"))
struct FAssetClassColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	// The path of the type
	UPROPERTY()
	FTopLevelAssetPath ClassPath;
};

// Column to store the disk size of an asset
USTRUCT(meta = (DisplayName = "Disk Size"))
struct FDiskSizeColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	// Size on disk (in bytes)
	UPROPERTY()
	int64 DiskSize = 0;
};


// Column to store the verse path of an asset
USTRUCT(meta = (DisplayName = "Verse Path"))
struct FVersePathColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	
	UE::Core::FVersePath VersePath;
};

// Used to notify the dependent queries of the update to the path of the row
USTRUCT()
struct FUpdatedPathTag : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

// Used to notify the dependent queries of a update to the asset data
USTRUCT()
struct FUpdatedAssetDataTag : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "CB Item Path"))
struct FVirtualPathColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (Searchable))
	FName VirtualPath;
};

USTRUCT(meta = (DisplayName = "Name"))
struct FItemNameColumn_Experimental : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (Searchable))
	FName Name;
};