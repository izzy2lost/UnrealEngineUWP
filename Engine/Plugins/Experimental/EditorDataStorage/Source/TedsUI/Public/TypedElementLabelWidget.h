// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementLabelWidget.generated.h"

UCLASS()
class UTypedElementLabelWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementLabelWidgetFactory() override = default;

	TEDSUI_API void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
	TEDSUI_API void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct FTypedElementLabelWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSUI_API FTypedElementLabelWidgetConstructor();
	~FTypedElementLabelWidgetConstructor() override = default;

	TEDSUI_API TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;

	TEDSUI_API TSharedPtr<SWidget> Construct(
		TypedElementRowHandle Row, 
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		const TypedElementDataStorage::FMetaDataView& Arguments) override;

protected:
	explicit FTypedElementLabelWidgetConstructor(const UScriptStruct* InTypeInfo);
	bool SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT(meta = (DisplayName = "Label widget"))
struct FTypedElementLabelWidgetColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	bool bShowHashInTooltip { false };
};