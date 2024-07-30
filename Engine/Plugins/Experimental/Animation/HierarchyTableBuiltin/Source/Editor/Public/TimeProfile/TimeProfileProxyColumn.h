// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementCommonTypes.h"

#include "TimeProfileProxyColumn.generated.h"

USTRUCT()
struct FHierarchyTableTimeColumn_StartTime final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT()
struct FHierarchyTableTimeColumn_EndTime final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT()
struct FHierarchyTableTimeColumn_TimeFactor final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT()
struct FHierarchyTableTimeColumn_Preview final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};