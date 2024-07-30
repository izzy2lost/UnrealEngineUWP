// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableType.h"

#include "HierarchyTableTypeTime.generated.h"

USTRUCT()
struct FHierarchyTableType_Time final : public FHierarchyTableType
{
	GENERATED_BODY()

	UPROPERTY()
	float StartTime;

	UPROPERTY()
	float EndTime;

	UPROPERTY()
	float TimeFactor;
};