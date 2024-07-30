// Copyright Epic Games, Inc. All Rights Reserved.

#include "HierarchyTableWidgetConstructor.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "HierarchyTable/Columns/OverrideColumn.h"

FHierarchyTableWidgetConstructor::FHierarchyTableWidgetConstructor(const UScriptStruct* InTypeInfo)
	: Super(InTypeInfo)
{
}

TSharedRef<SWidget> FHierarchyTableWidgetConstructor::CreateWidget(FHierarchyTableEntryData* EntryData)
{
	return SNullWidget::NullWidget;
}

TSharedPtr<SWidget> FHierarchyTableWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center);
}

bool FHierarchyTableWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	TSharedPtr<SBox> WidgetInstance = StaticCastSharedPtr<SBox>(Widget);

	// Row is not actually the row we want, its contained inside of a row reference, I don't know why things are done this way.
	TypedElementDataStorage::RowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;
	FTypedElementOverrideColumn* OverrideColumn = DataStorage->GetColumn<FTypedElementOverrideColumn>(TargetRow);

	TSharedRef<SWidget> ActualWidget = CreateWidget(OverrideColumn->OwnerEntry);
	WidgetInstance->SetContent(ActualWidget);

	return true;
}