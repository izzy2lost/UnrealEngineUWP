// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "SlateColorWidget.generated.h"

class ITypedElementDataStorageInterface;
class UScriptStruct;

UCLASS()
class USlateColorWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~USlateColorWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

// Widget to show and edit the color column in TEDS
USTRUCT()
struct FSlateColorWidgetConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	FSlateColorWidgetConstructor();
	~FSlateColorWidgetConstructor() override = default;

	virtual TSharedPtr<SWidget> CreateWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		RowHandle TargetRow,
		RowHandle WidgetRow, 
		const UE::Editor::DataStorage::FMetaDataView& Arguments) override;
};