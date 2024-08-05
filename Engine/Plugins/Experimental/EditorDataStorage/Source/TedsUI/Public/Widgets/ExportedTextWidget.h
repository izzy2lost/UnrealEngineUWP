// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Elements/Common/TypedElementQueryConditions.h"

#include "ExportedTextWidget.generated.h"

class ITypedElementDataStorageInterface;
class UScriptStruct;

UCLASS()
class TEDSUI_API UExportedTextWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	virtual ~UExportedTextWidgetFactory() override = default;

	virtual void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;

	TSet<TWeakObjectPtr<const UScriptStruct>> RegisteredTypes;
};

USTRUCT()
struct TEDSUI_API FExportedTextWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FExportedTextWidgetConstructor();
	virtual ~FExportedTextWidgetConstructor() override = default;

	virtual TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;
	virtual const TypedElementDataStorage::FQueryConditions* GetQueryConditions() const override;
	virtual FString CreateWidgetDisplayName(
		ITypedElementDataStorageInterface* DataStorage, TypedElementDataStorage::RowHandle Row) const override;

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
struct TEDSUI_API FExportedTextWidgetTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};