// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"

FTypedElementWidgetConstructor::FTypedElementWidgetConstructor(const UScriptStruct* InTypeInfo)
	: TypeInfo(InTypeInfo)
{
}

const UScriptStruct* FTypedElementWidgetConstructor::GetTypeInfo() const
{
	return TypeInfo;
}

TConstArrayView<const UScriptStruct*> FTypedElementWidgetConstructor::GetAdditionalColumnsList() const
{
	return {};
}

bool FTypedElementWidgetConstructor::CanBeReused() const
{
	return true;
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