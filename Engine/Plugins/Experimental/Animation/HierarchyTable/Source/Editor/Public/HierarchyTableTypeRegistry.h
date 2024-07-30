// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Common/TypedElementHandles.h"
#include "HierarchyTableType.h"
#include "StructUtils/InstancedStruct.h"
#include "HierarchyTableTypeRegistry.generated.h"

class UHierarchyTable;
class UScriptStruct;
struct FHierarchyTableEntryData;

UCLASS(Abstract)
class HIERARCHYTABLEEDITOR_API UHierarchyTableTypeHandler_Base : public UObject
{
	GENERATED_BODY()

public:
	virtual FInstancedStruct GetDefaultEntry() const { return FInstancedStruct(); }

	virtual void InitializeTable(TObjectPtr<class UHierarchyTable> InHierarchyTable) const {};

	virtual TArray<UScriptStruct*> GetColumns() const { return {}; };
};

UCLASS()
class HIERARCHYTABLEEDITOR_API UHierarchyTableTypeRegistry : public UObject
{
	GENERATED_BODY()

public:
	void Register(const UScriptStruct* HierarchyTableType, const UHierarchyTableTypeHandler_Base* Handler);
	void Unregister(const UScriptStruct* HierarchyTableType);

	const UHierarchyTableTypeHandler_Base* FindHandler(const UScriptStruct* HierarchyTableType) const;

private:
	TMap<const UScriptStruct*, const UHierarchyTableTypeHandler_Base*> Handlers;
};