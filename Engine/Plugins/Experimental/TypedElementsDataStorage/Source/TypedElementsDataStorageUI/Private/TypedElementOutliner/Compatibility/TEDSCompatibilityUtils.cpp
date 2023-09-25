// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutliner/Compatibility/TEDSCompatibilityUtils.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "TypedElementOutliner/TypedElementOutlinerColumnIntegration.h"
#include "Widgets/SWidget.h"

FBaseTEDSOutlinerMode::FBaseTEDSOutlinerMode()
: WidgetPurposes{TEXT("SceneOutliner.ItemLabel.Cell"), TEXT("SceneOutliner.Cell"), TEXT("General.Cell")}
{
	// Initialize the TEDS constructs
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("Unable to initialize the Typed Elements Outliner before TEDS is initialized."));
	if (Registry)
	{
		Storage = Registry->GetMutableDataStorage();
		StorageUi = Registry->GetMutableDataStorageUi();
		StorageCompatibility = Registry->GetMutableDataStorageCompatibility();
	}

	using namespace TypedElementQueryBuilder;

	auto CreateWidgetConstructorForQuery = [this](TypedElementDataStorage::FQueryDescription InQueryDescription) -> TSharedPtr<FTypedElementWidgetConstructor>
	{
		// Create the Widget Constructor for the item label column
		using MatchApproach = ITypedElementDataStorageUiInterface::EMatchApproach;

		// Make a copy of the columns because CreateWidgetConstructors can modify it
		TArray<TWeakObjectPtr<const UScriptStruct>> ColumnTypes(InQueryDescription.SelectionTypes);

		TSharedPtr<FTypedElementWidgetConstructor> OutWidgetConstructorPtr;

		bool bFoundWidget = false;
		
		for(const FName& WidgetPurpose : WidgetPurposes)
		{
			StorageUi->CreateWidgetConstructors(WidgetPurpose, MatchApproach::ExactMatch, ColumnTypes, {},
				[&OutWidgetConstructorPtr, ColumnTypes, &bFoundWidget](
				TUniquePtr<FTypedElementWidgetConstructor> CreatedConstructor, 
				TConstArrayView<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes)
				{
					if (ColumnTypes.Num() == MatchedColumnTypes.Num())
					{
						OutWidgetConstructorPtr = TSharedPtr<FTypedElementWidgetConstructor>(CreatedConstructor.Release());
						bFoundWidget = true;
					}
					// Either this was the exact match so no need to search further or the longest possible chain didn't match so the next ones will 
					// always be shorter in both cases just return.
					return false;
				});

			if(bFoundWidget)
			{
				break;
			}
		}

		return OutWidgetConstructorPtr;
	};

	TypedElementDataStorage::FQueryDescription TypeColumnQueryDescription = TypedElementDataStorage::FQueryDescription(Select()
									.ReadOnly<FTypedElementClassTypeInfoColumn>()
									//.ReadOnly<FPrefabOverrideTag>()
									.Compile());

	if(TSharedPtr<FTypedElementWidgetConstructor> TypeColumnWidgetConstructor = CreateWidgetConstructorForQuery(TypeColumnQueryDescription))
	{
		QueryToWidgetConstructorMap.Emplace(TypeColumnQueryDescription, TypeColumnWidgetConstructor);
	}

	TypedElementDataStorage::FQueryDescription LabelColumnQueryDescription = TypedElementDataStorage::FQueryDescription(Select()
								.ReadWrite<FTypedElementLabelColumn>()
								.Compile());

	if(TSharedPtr<FTypedElementWidgetConstructor> LabelColumnWidgetConstructor = CreateWidgetConstructorForQuery(LabelColumnQueryDescription))
	{
		QueryToWidgetConstructorMap.Emplace(LabelColumnQueryDescription, LabelColumnWidgetConstructor);
	}

}

TSharedRef<SWidget> FBaseTEDSOutlinerMode::CreateLabelWidgetForItem(TypedElementRowHandle InRowHandle)
{
	auto CreateWidgetForQuery = [InRowHandle, this](const TPair<TypedElementDataStorage::FQueryDescription, TSharedPtr<FTypedElementWidgetConstructor>>& QueryConstructorPair) -> TSharedPtr<SWidget>
	{
		// Create a generic metadata view for the Type Widget
		TypedElementDataStorage::FMetaData QueryWideMetaData;
		QueryWideMetaData.AddImmutableData("TypedElementTypeInfoWidget_bUseIcon", true);
		TypedElementDataStorage::FGenericMetaDataView GenericMetaDataView(QueryWideMetaData);

		// Create metadata for the query itself
		TypedElementDataStorage::FQueryMetaDataView QueryMetaDataView(QueryConstructorPair.Key);

		// Combine the two metadata
		TypedElementDataStorage::FComboMetaDataView MetaDataArgs(GenericMetaDataView, QueryMetaDataView);

		TArray<TWeakObjectPtr<const UScriptStruct>> ColumnTypes(QueryConstructorPair.Key.SelectionTypes);
		TSharedPtr<FTypedElementWidgetConstructor> CellWidgetConstructor = QueryConstructorPair.Value;

		if (InRowHandle != TypedElementInvalidRowHandle && Storage->HasColumns(InRowHandle, QueryConstructorPair.Key.SelectionTypes))
		{
			TypedElementRowHandle UiRowHandle = Storage->AddRow(Storage->FindTable(FTypedElementSceneOutlinerQueryBinder::CellWidgetTableName));
			
			Storage->AddColumns(UiRowHandle, CellWidgetConstructor->GetAdditionalColumnsList());
			
			if (ColumnTypes.Num() == 1)
			{
				if (FTypedElementScriptStructTypeInfoColumn* TypeInfo = Storage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(UiRowHandle))
				{
					TypeInfo->TypeInfo = *ColumnTypes.begin();
				}
			}
			if (FTypedElementRowReferenceColumn* RowReference = Storage->GetColumn<FTypedElementRowReferenceColumn>(UiRowHandle))
			{
				RowReference->Row = InRowHandle;
			}

			if (TSharedPtr<SWidget> Widget = StorageUi->ConstructWidget(UiRowHandle, *CellWidgetConstructor, MetaDataArgs))
			{
				return Widget.ToSharedRef();
			}
			else
			{
				Storage->RemoveRow(InRowHandle);
			}
		}
		return nullptr;
	};
	
	TSharedRef<SHorizontalBox> CombinedWidget = SNew(SHorizontalBox);

	for(const TPair<TypedElementDataStorage::FQueryDescription, TSharedPtr<FTypedElementWidgetConstructor>>& QueryConstructorPair : QueryToWidgetConstructorMap)
	{
		if(TSharedPtr<SWidget> WidgetForQuery = CreateWidgetForQuery(QueryConstructorPair))
		{
			CombinedWidget->AddSlot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(2.0f, 0.0f, 4.0f, 0.0f)
					[
						WidgetForQuery.ToSharedRef()
					];
		}
	}

	
	

	return CombinedWidget;
}

ITypedElementDataStorageInterface* FBaseTEDSOutlinerMode::GetStorage()
{
	return Storage;
}

ITypedElementDataStorageUiInterface* FBaseTEDSOutlinerMode::GetStorageUI()
{
	return StorageUi;
}

ITypedElementDataStorageCompatibilityInterface* FBaseTEDSOutlinerMode::GetStorageCompatibility()
{
	return StorageCompatibility;
}