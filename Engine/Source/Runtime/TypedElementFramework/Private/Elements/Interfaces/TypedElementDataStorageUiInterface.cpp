// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"

FTypedElementWidgetConstructor::FTypedElementWidgetConstructor(const UScriptStruct* InTypeInfo)
	: TypeInfo(InTypeInfo)
{
}

bool FTypedElementWidgetConstructor::Initialize(const TypedElementDataStorage::FMetaDataView& InArguments,
	TArray<TWeakObjectPtr<const UScriptStruct>> InMatchedColumnTypes, const TypedElementDataStorage::FQueryConditions& InQueryConditions)
{
	MatchedColumnTypes = MoveTemp(InMatchedColumnTypes);
	QueryConditions = &InQueryConditions;
	return true;
}

const UScriptStruct* FTypedElementWidgetConstructor::GetTypeInfo() const
{
	return TypeInfo;
}

const TArray<TWeakObjectPtr<const UScriptStruct>>& FTypedElementWidgetConstructor::GetMatchedColumns() const
{
	return MatchedColumnTypes;
}

const TypedElementDataStorage::FQueryConditions* FTypedElementWidgetConstructor::GetQueryConditions() const
{
	return QueryConditions;
}

TConstArrayView<const UScriptStruct*> FTypedElementWidgetConstructor::GetAdditionalColumnsList() const
{
	return {};
}

TSharedPtr<SWidget> FTypedElementWidgetConstructor::Construct(
	TypedElementRowHandle Row,
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	TSharedPtr<SWidget> Widget = CreateWidget(Arguments);
	if (Widget)
	{
		DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row)->Widget = Widget;
		if (SetColumns(DataStorage, Row))
		{
			if (FinalizeWidget(DataStorage, DataStorageUi, Row, Widget))
			{
				return Widget;
			}
		}
	}
	return nullptr;
}

TSharedPtr<SWidget> FTypedElementWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return nullptr;
}

bool FTypedElementWidgetConstructor::SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row)
{
	return true;
}

bool FTypedElementWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	return true;
}