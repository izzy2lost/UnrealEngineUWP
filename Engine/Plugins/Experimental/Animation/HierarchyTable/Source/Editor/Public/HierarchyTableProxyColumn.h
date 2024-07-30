// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementCommonTypes.h"

#include "HierarchyTableProxyColumn.generated.h"

class UHierarchyTable;
struct FHierarchyTableEntryData;

USTRUCT()
struct FHierarchyTableProxyColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

public:
	UHierarchyTable* OwnerTable;

	FHierarchyTableEntryData* OwnerEntry;
};