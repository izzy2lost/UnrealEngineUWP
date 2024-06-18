// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementTestColumns.generated.h"

USTRUCT(meta = (DisplayName = "ColumnA"))
struct FTestColumnA final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnB"))
struct FTestColumnB final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnC"))
struct FTestColumnC final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnD"))
struct FTestColumnD final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnE"))
struct FTestColumnE final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnF"))
struct FTestColumnF final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnG"))
struct FTestColumnG final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnInt"))
struct FTestColumnInt final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	int TestInt = 0;
};

USTRUCT(meta = (DisplayName = "ColumnString"))
struct FTestColumnString final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
    FString TestString;
};


USTRUCT(meta = (DisplayName = "TagA"))
struct FTestTagColumnA final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagB"))
struct FTestTagColumnB final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagC"))
struct FTestTagColumnC final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagD"))
struct FTestTagColumnD final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TestReferenceColumn"))
struct FTEDSProcessorTestsReferenceColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	// UPROPERTY()
	TypedElementDataStorage::RowHandle Reference = TypedElementDataStorage::InvalidRowHandle;

	bool IsReferenced = false;
};

USTRUCT(meta = (DisplayName = "TEDSProcessorTests_PrimaryTag"))
struct FTEDSProcessorTests_PrimaryTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TEDSProcessorTestsSecondaryTag"))
struct FTEDSProcessorTests_SecondaryTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TEDSProcessorTests_Linked"))
struct FTEDSProcessorTests_Linked final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};