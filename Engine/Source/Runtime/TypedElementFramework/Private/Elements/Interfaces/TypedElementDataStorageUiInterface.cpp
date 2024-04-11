// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementDataStorageWidget.h"

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

TSharedPtr<SWidget> FTypedElementWidgetConstructor::ConstructFinalWidget(
	TypedElementRowHandle Row,
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	// Add the additional columns to the UI row
	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
	DataStorage->AddColumns(Row, GetAdditionalColumnsList());
	
	if (const FTypedElementRowReferenceColumn* RowReference = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row))
	{
		// If the original row matches this widgets query conditions currently, create the actual internal widget
		if (DataStorage->HasRowBeenAssigned(RowReference->Row) &&
			GetQueryConditions() &&
			DataStorage->MatchesColumns(RowReference->Row, *GetQueryConditions()))
		{
			Widget = Construct(Row, DataStorage, DataStorageUi, Arguments);
		}
	}
	// If we don't have an original row, simply construct the widget
	else
	{
		Widget = Construct(Row, DataStorage, DataStorageUi, Arguments);
	}

	// Create a container widget to hold the content (even if it doesn't exist yet)
	TSharedPtr<STedsWidget> ContainerWidget = SNew(STedsWidget)
	.UiRowHandle(Row)
	.ConstructorTypeInfo(TypeInfo)
	[
		Widget.ToSharedRef()
	];
	
	DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row)->TedsWidget = ContainerWidget;
	return ContainerWidget;
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
				SetupDebugColumns(Row, DataStorage, DataStorageUi, Widget);
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

FString FTypedElementWidgetConstructor::GetWidgetLabel(const TSharedPtr<SWidget>& Widget)
{
	// The default widget label is simply the type of the widget
	return Widget->GetType().ToString();
}

bool FTypedElementWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	return true;
}

void FTypedElementWidgetConstructor::SetupDebugColumns(TypedElementRowHandle Row, ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi, const TSharedPtr<SWidget>& Widget)
{
	bool bAddLabelColumn = true;

	const FString WidgetLabel(GetWidgetLabel(Widget));

	// Only add the label column for widgets that are not created for other widgets. This is to prevent recursively displaying widgets for widgets
	// in the TEDS-Debugger - we only want top level widgets to be displayed
	if (const FTypedElementRowReferenceColumn* RowReference = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row))
	{
		if (DataStorage->HasColumns<FTypedElementSlateWidgetReferenceColumn>(RowReference->Row))
		{
			bAddLabelColumn = false;
		}
	}

	if(bAddLabelColumn)
	{
		DataStorage->AddColumn(Row, FTypedElementLabelColumn{.Label = WidgetLabel} );
	}
}
