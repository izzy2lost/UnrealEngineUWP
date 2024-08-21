// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "OutlinerLabelWidget.generated.h"

class ITypedElementDataStorageInterface;
class UScriptStruct;

UCLASS()
class UOutlinerLabelWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UOutlinerLabelWidgetFactory() override = default;

	TEDSOUTLINER_API void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

// Label widget for the Scene Outliner that shows an icon (with optional override information) + a text label
USTRUCT()
struct FOutlinerLabelWidgetConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSOUTLINER_API FOutlinerLabelWidgetConstructor();
	~FOutlinerLabelWidgetConstructor() override = default;

	TEDSOUTLINER_API virtual TSharedPtr<SWidget> CreateWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		UE::Editor::DataStorage::RowHandle TargetRow,
		UE::Editor::DataStorage::RowHandle WidgetRow, 
		const UE::Editor::DataStorage::FMetaDataView& Arguments) override;

protected:

	TSharedRef<SWidget> CreateLabel(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, RowHandle TargetRow, RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments);
};