// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementDataStorageWidget.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"

STedsWidget::STedsWidget()
	: UiRowHandle(TypedElementDataStorage::InvalidRowHandle)
{

}

void STedsWidget::Construct(const FArguments& InArgs)
{
	UiRowHandle = InArgs._UiRowHandle;

	// If the Ui row wasn't already registered externally, register it with Teds
	if(UiRowHandle == TypedElementDataStorage::InvalidRowHandle)
	{
		RegisterTedsWidget(InArgs._Content.Widget);
	}

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void STedsWidget::RegisterTedsWidget(const TSharedPtr<SWidget>& InContentWidget)
{
	ITypedElementDataStorageInterface* Storage = GetStorageIfAvailable();

	// If TEDS is not enabled, STedsWidget will just behave like a regular widget
	if(!Storage)
	{
		return;
	}
	
	const TypedElementDataStorage::TableHandle WidgetTable = Storage->FindTable(TEXT("Editor_WidgetTable"));
	if(WidgetTable == TypedElementDataStorage::InvalidTableHandle)
	{
		return;
	}
	
	UiRowHandle = Storage->AddRow(WidgetTable);
	
	if(FTypedElementSlateWidgetReferenceColumn* WidgetReferenceColumn = Storage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(UiRowHandle))
	{
		WidgetReferenceColumn->TedsWidget = SharedThis(this);
		WidgetReferenceColumn->Widget = InContentWidget;
	}
}

void STedsWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	if(ITypedElementDataStorageInterface* Storage = GetStorageIfAvailable())
	{
		if(FTypedElementSlateWidgetReferenceColumn* WidgetReferenceColumn = Storage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(UiRowHandle))
		{
			WidgetReferenceColumn->Widget = InContent;
		}
	}
	
	ChildSlot
	[
		InContent
	];
}

TypedElementDataStorage::RowHandle STedsWidget::GetRowHandle() const
{
	return UiRowHandle;
}

ITypedElementDataStorageInterface* STedsWidget::GetStorageIfAvailable()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	if(!Registry || !Registry->AreDataStorageInterfacesSet())
	{
		return nullptr;
	}

	return Registry->GetMutableDataStorage();
}
