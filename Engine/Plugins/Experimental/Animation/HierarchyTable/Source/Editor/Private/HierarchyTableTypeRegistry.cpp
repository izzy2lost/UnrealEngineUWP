// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableTypeRegistry.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HierarchyTable/Columns/OverrideColumn.h"
#include "HierarchyTable.h"

void UHierarchyTableTypeRegistry::Register(const UScriptStruct* HierarchyTableType, const UHierarchyTableTypeHandler_Base* Handler)
{
	Handlers.Add(HierarchyTableType, Handler);
}

void UHierarchyTableTypeRegistry::Unregister(const UScriptStruct* HierarchyTableType)
{
	Handlers.Remove(HierarchyTableType);
}

const UHierarchyTableTypeHandler_Base* UHierarchyTableTypeRegistry::FindHandler(const UScriptStruct* HierarchyTableType) const
{
	const UHierarchyTableTypeHandler_Base* const* Result = Handlers.Find(HierarchyTableType);
	if (Result)
	{
		return *Result;
	}
	return nullptr;
}