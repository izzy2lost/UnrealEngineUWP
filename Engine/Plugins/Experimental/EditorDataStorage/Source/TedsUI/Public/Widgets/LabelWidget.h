// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "LabelWidget.generated.h"

class ITypedElementDataStorageInterface;
class UScriptStruct;

UCLASS()
class ULabelWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~ULabelWidgetFactory() override = default;

	TEDSUI_API void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
	TEDSUI_API void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct FLabelWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSUI_API FLabelWidgetConstructor();
	~FLabelWidgetConstructor() override = default;

	TEDSUI_API TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;

	TEDSUI_API TSharedPtr<SWidget> Construct(
		TypedElementRowHandle Row, 
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		const TypedElementDataStorage::FMetaDataView& Arguments) override;

protected:
	explicit FLabelWidgetConstructor(const UScriptStruct* InTypeInfo);
	bool SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT(meta = (DisplayName = "Label widget"))
struct FLabelWidgetColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	bool bShowHashInTooltip { false };
};