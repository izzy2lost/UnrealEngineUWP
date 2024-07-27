// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementExportedTextWidget.generated.h"

UCLASS()
class TEDSUI_API UTypedElementExportedTextWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	virtual ~UTypedElementExportedTextWidgetFactory() override = default;

	virtual void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;

	TSet<TWeakObjectPtr<const UScriptStruct>> RegisteredTypes;
};

USTRUCT()
struct TEDSUI_API FTypedElementExportedTextWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementExportedTextWidgetConstructor();
	virtual ~FTypedElementExportedTextWidgetConstructor() override = default;

	virtual TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;
	virtual const TypedElementDataStorage::FQueryConditions* GetQueryConditions() const override;

protected:
	virtual TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	virtual bool FinalizeWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row,
		const TSharedPtr<SWidget>& Widget) override;

protected:
	// The column this exported text widget is operating on
	TypedElementDataStorage::FQueryConditions MatchedColumn;
};

USTRUCT(meta = (DisplayName = "Exported text widget"))
struct TEDSUI_API FTypedElementExportedTextWidgetTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};