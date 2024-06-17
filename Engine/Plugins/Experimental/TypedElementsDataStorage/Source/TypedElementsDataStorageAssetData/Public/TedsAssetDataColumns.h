// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Elements/Common/TypedElementCommonTypes.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"

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
