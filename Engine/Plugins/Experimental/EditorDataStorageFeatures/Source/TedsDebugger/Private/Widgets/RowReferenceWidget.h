// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "RowReferenceWidget.generated.h"

class ITypedElementDataStorageInterface;
class SWidget;

/*
 * Widget for the TEDS Debugger that visualizes a reference to another row
 */
UCLASS()
class URowReferenceWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~URowReferenceWidgetFactory() override;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
	
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
};

USTRUCT()
struct FRowReferenceWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FRowReferenceWidgetConstructor();
	~FRowReferenceWidgetConstructor() override = default;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};