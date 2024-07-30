// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableType.h"

#include "HierarchyTableTypeMask.generated.h"

USTRUCT()
struct FHierarchyTableType_Mask final : public FHierarchyTableType
{
	GENERATED_BODY()

	UPROPERTY()
	float Value;
};