// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "TypedElementRowReferenceWidget.generated.h"

/*
 * Widget for the TEDS Debugger that visualizes a reference to another row
 */
UCLASS()
class UTypedElementRowReferenceWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementRowReferenceWidgetFactory() override;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
	
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
};

USTRUCT()
struct FTypedElementRowReferenceWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementRowReferenceWidgetConstructor();
	~FTypedElementRowReferenceWidgetConstructor() override = default;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};