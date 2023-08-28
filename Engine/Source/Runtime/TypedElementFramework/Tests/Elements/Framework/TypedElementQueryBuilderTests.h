// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementQueryBuilderTests.generated.h"

USTRUCT(meta = (DisplayName = "A"))
struct FTestColumnA final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "B"))
struct FTestColumnB final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "C"))
struct FTestColumnC final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "D"))
struct FTestColumnD final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "E"))
struct FTestColumnE final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "F"))
struct FTestColumnF final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "G"))
struct FTestColumnG final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};
