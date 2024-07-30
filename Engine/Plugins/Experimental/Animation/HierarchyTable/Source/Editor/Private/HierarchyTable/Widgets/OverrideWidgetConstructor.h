// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "OverrideWidgetConstructor.generated.h"

// TODO: Can be converted into a child of FHierarchyTableWidgetConstructor
USTRUCT()
struct FTypedElementWidgetConstructor_Override : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementWidgetConstructor_Override();
	~FTypedElementWidgetConstructor_Override() override = default;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments);
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};