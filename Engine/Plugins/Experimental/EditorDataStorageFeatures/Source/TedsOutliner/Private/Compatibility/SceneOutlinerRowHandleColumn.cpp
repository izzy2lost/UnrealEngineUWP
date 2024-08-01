// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/SceneOutlinerRowHandleColumn.h"

#include "SortHelper.h"
#include "TedsOutlinerItem.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "TedsTableViewerColumn.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerRowHandleColumn"

FSceneOutlinerRowHandleColumn::FSceneOutlinerRowHandleColumn(ISceneOutliner& SceneOutliner)
	: WeakSceneOutliner(StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared()))
{
	auto AssignWidgetToColumn = [this](TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)
	{
		TSharedPtr<FTypedElementWidgetConstructor> WidgetConstructor(Constructor.Release());
		TableViewerColumn = MakeShared<UE::EditorDataStorage::FTedsTableViewerColumn>(TEXT("Row Handle"), WidgetConstructor);
		return false;
	};
	
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("FTedsOutlinerRowHandleColumn created before UTypedElementRegistry is available."));
	ITypedElementDataStorageUiInterface* StorageUi = Registry->GetMutableDataStorageUi();
	checkf(StorageUi, TEXT("FTedsOutlinerRowHandleColumn created before data storage interfaces were initialized."))

	StorageUi->CreateWidgetConstructors(TEXT("General.Cell.RowHandle"), TypedElementDataStorage::FMetaDataView(), AssignWidgetToColumn);
}


FName FSceneOutlinerRowHandleColumn::GetID()
{
	static const FName ID("Row Handle");
	return ID;
}

FName FSceneOutlinerRowHandleColumn::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FSceneOutlinerRowHandleColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetID())
	.FillWidth(2)
	.HeaderComboVisibility(EHeaderComboVisibility::OnHover);
}

const TSharedRef<SWidget> FSceneOutlinerRowHandleColumn::ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
	auto SceneOutliner = WeakSceneOutliner.Pin();
	check(SceneOutliner.IsValid());

	if (const FTedsOutlinerTreeItem* OutlinerTreeItem = TreeItem->CastTo<FTedsOutlinerTreeItem>())
	{
		const TypedElementDataStorage::RowHandle RowHandle = OutlinerTreeItem->GetRowHandle();

		if(TSharedPtr<SWidget> Widget = TableViewerColumn->ConstructRowWidget(RowHandle))
		{
			return Widget.ToSharedRef();
		}
	}
	return SNullWidget::NullWidget;
}

void FSceneOutlinerRowHandleColumn::PopulateSearchStrings(const ISceneOutlinerTreeItem& Item, TArray<FString>& OutSearchStrings) const
{
	if (const FTedsOutlinerTreeItem* OutlinerTreeItem = Item.CastTo<FTedsOutlinerTreeItem>())
	{
		OutSearchStrings.Add(LexToString<FString>(OutlinerTreeItem->GetRowHandle()));
	}

}

void FSceneOutlinerRowHandleColumn::SortItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const
{
	FSceneOutlinerSortHelper<TypedElementDataStorage::RowHandle>()
		/** Sort by type first */
		.Primary([this](const ISceneOutlinerTreeItem& Item)
		{
			if (const FTedsOutlinerTreeItem* OutlinerTreeItem = Item.CastTo<FTedsOutlinerTreeItem>())
			{
				return OutlinerTreeItem->GetRowHandle();
			}

			return TypedElementDataStorage::InvalidRowHandle;
		}, SortMode)
		.Sort(OutItems);
}

#undef LOCTEXT_NAMESPACE
