// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "HierarchyTableWidgetConstructor.generated.h"

struct FHierarchyTableEntryData;

USTRUCT()
struct HIERARCHYTABLEEDITOR_API FHierarchyTableWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	explicit FHierarchyTableWidgetConstructor() : Super(StaticStruct()) {}
	explicit FHierarchyTableWidgetConstructor(const UScriptStruct* InTypeInfo);

	virtual ~FHierarchyTableWidgetConstructor() override = default;

	virtual TSharedRef<SWidget> CreateWidget(FHierarchyTableEntryData* EntryData);

protected:
	// Begin FHierarchyTableWidgetConstructor
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments);

	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;
	// End FHierarchyTableWidgetConstructor
};