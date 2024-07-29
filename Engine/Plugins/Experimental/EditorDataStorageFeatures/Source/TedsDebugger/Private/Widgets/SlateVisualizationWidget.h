// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "SlateVisualizationWidget.generated.h"

class ITypedElementDataStorageInterface;
class SWidget;

/*
 * Widget for the TEDS Debugger that shows a slate widget reference
 */
UCLASS()
class USlateVisualizationWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~USlateVisualizationWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct FSlateVisualizationWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FSlateVisualizationWidgetConstructor();
	~FSlateVisualizationWidgetConstructor() override = default;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};