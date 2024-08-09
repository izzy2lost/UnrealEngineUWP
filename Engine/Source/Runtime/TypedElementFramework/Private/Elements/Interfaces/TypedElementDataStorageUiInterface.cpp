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

FString FTypedElementWidgetConstructor::CreateWidgetDisplayName(
	ITypedElementDataStorageInterface* DataStorage, TypedElementDataStorage::RowHandle Row) const
{
	switch (MatchedColumnTypes.Num())
	{
	case 0:
		return FString(TEXT("TEDS Column"));
	case 1:
		return DescribeColumnType(MatchedColumnTypes[0].Get());
	default:
	{
		FString LongestMatchString = DescribeColumnType(MatchedColumnTypes[0].Get());
		FStringView LongestMatch = LongestMatchString;
		const TWeakObjectPtr<const UScriptStruct>* It = MatchedColumnTypes.GetData();
		const TWeakObjectPtr<const UScriptStruct>* ItEnd = It + MatchedColumnTypes.Num();
		++It; // Skip the first entry as that's already set.
		for (; It != ItEnd; ++It)
		{
			FString NextMatchText = DescribeColumnType(It->Get());
			FStringView NextMatch = NextMatchText;

			int32 MatchSize = 0;
			auto ItLeft = LongestMatch.begin();
			auto ItLeftEnd = LongestMatch.end();
			auto ItRight = NextMatch.begin();
			auto ItRightEnd = NextMatch.end();
			while (
				ItLeft != ItLeftEnd &&
				ItRight != ItRightEnd &&
				*ItLeft == *ItRight)
			{
				++MatchSize;
				++ItLeft;
				++ItRight;
			}

			// At least 3 letters have to match to avoid single or double letter names which typically mean nothing.
			if (MatchSize > 2)
			{
				LongestMatch.LeftInline(MatchSize);
			}
			else
			{
				// There are not enough characters in the string that match. Just return the name of the first column
				return LongestMatchString;
			}
		}
		return FString(LongestMatch);
	}
	};
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
		bool bConstructWidget = DataStorage->IsRowAssigned(RowReference->Row);

		// If the original row matches this widgets query conditions currently, create the actual internal widget
		if (QueryConditions)
		{
			bConstructWidget &= DataStorage->MatchesColumns(RowReference->Row, *QueryConditions);
		}
		
		if (bConstructWidget)
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
				AddDefaultWidgetColumns(Row, DataStorage);
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

FString FTypedElementWidgetConstructor::DescribeColumnType(const UScriptStruct* ColumnType) const
{
	static const FName DisplayNameName(TEXT("DisplayName"));

#if WITH_EDITOR
	if (ColumnType)
	{
		const FString* Name = ColumnType->FindMetaData(DisplayNameName);
		return Name ? *Name : ColumnType->GetDisplayNameText().ToString();
	}
	else
#endif
	{
		return FString(TEXT("<Invalid>"));
	}
}

bool FTypedElementWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	return true;
}

void FTypedElementWidgetConstructor::AddDefaultWidgetColumns(TypedElementRowHandle Row, ITypedElementDataStorageInterface* DataStorage) const
{
	const FString WidgetLabel(CreateWidgetDisplayName(DataStorage, Row));
	DataStorage->AddColumn(Row, FTypedElementLabelColumn{.Label = WidgetLabel} );
}
