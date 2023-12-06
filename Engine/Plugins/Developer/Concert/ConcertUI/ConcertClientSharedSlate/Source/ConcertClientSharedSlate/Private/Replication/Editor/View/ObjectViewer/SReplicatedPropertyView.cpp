// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicatedPropertyView.h"

#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/View/ObjectViewer/Property/SPropertyTreeView.h"
#include "Replication/Editor/View/Tree/SelectionViewerColumns.h"

#include "Algo/AllOf.h"
#include "Algo/ForEach.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicatedPropertyView"

namespace UE::ConcertClientSharedSlate
{
	void SReplicatedPropertyView::Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel)
	{
		PropertiesModel = MoveTemp(InPropertiesModel);
		GetSelectedRootObjectsDelegate = InArgs._GetSelectedRootObjects;
		check(GetSelectedRootObjectsDelegate.IsBound());
		
		ChildSlot
		[
			CreatePropertiesView(InArgs)
		];
	}
	
	void SReplicatedPropertyView::RefreshPropertyData()
	{
		const TArray<FSoftObjectPath> SelectedObjects = GetObjectsSelectedForPropertyEditing();
		if (SelectedObjects.IsEmpty())
		{
			SetPropertyContent(EReplicatedPropertyContent::NoSelection);
			return;
		}

		// Technically, the classes just need to be compatible with each other... but it is easier to just allow the same class.
		const TOptional<FSoftClassPath> SharedClass = GetClassForPropertiesFromSelection(SelectedObjects);
		if (!SharedClass)
		{
			SetPropertyContent(EReplicatedPropertyContent::SelectionTooBig);
			return;
		}
		const FSoftClassPath Class = *SharedClass;
		if (!ensureMsgf(Class.IsValid(), TEXT("This should only trigger if IReplicationSubobjectView returned a object not contained the model & that the UI is view-only. In that case fix your IReplicationSubobjectView or use it in an UI editor.")))
		{
			return;
		}

		// Build the set of properties that are shared by all of the selected objects
		TSet<FConcertPropertyChain> SharedProperties = PropertiesModel->GetAllProperties(SelectedObjects[0]);
		for (int32 i = 1; i < SelectedObjects.Num(); ++i)
		{
			SharedProperties = PropertiesModel->GetAllProperties(SelectedObjects[i]).Union(SharedProperties);
		}
		
		// Try to re-use old instances by using the old ChainToPropertyDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> NewChainToPropertyDataCache;
		
		PropertyRowData.Empty();
		for (const FConcertPropertyChain& PropertyChain : SharedProperties)
		{
			const TSharedPtr<FReplicatedPropertyData>* ExistingItem = ChainToPropertyDataCache.Find(PropertyChain);
			const TSharedRef<FReplicatedPropertyData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocatePropertyData(Class, PropertyChain);
			PropertyRowData.Emplace(Item);
			NewChainToPropertyDataCache.Emplace(PropertyChain, Item);
		}

		// If an item was removed, then NewPathToPropertyDataCache does not contain it. 
		ChainToPropertyDataCache = MoveTemp(NewChainToPropertyDataCache);
		
		// The tree view requires the item source to only contain the root items.
		BuildRootPropertyRowData();
		SetPropertyContent(EReplicatedPropertyContent::Properties);
		ReplicatedProperties->OnItemsChanged();
	}

	void SReplicatedPropertyView::RequestResortForColumn(const FName& ColumnId)
	{
		ReplicatedProperties->RequestResortForColumn(ColumnId);
	}

	TArray<FSoftObjectPath> SReplicatedPropertyView::GetObjectsSelectedForPropertyEditing() const
	{
		TArray<FSoftObjectPath> Result;
		Algo::Transform(GetSelectedRootObjectsDelegate.Execute(), Result, [](const TSharedPtr<FReplicatedObjectData>& ObjectData)
		{
			return ObjectData->GetObjectPath();
		});
		return Result;
	}

	TSharedRef<SWidget> SReplicatedPropertyView::CreatePropertiesView(const FArguments& InArgs)
	{
		TArray Columns
		{
			ReplicationColumns::Property::LabelColumn(),
			ReplicationColumns::Property::TypeColumn()
		};
		Columns.Append(InArgs._AdditionalPropertyColumns);

		// Set both primary and secondary in case one is overriden but always use the override.
		const FColumnSortInfo PrimarySort = InArgs._PrimarySort.IsValid()
			? InArgs._PrimarySort
			: FColumnSortInfo{ ReplicationColumns::TopLevel::LabelColumnId, EColumnSortMode::Ascending };
		const FColumnSortInfo SecondarySort = InArgs._SecondarySort.IsValid()
			? InArgs._SecondarySort
			: FColumnSortInfo{ ReplicationColumns::TopLevel::LabelColumnId, EColumnSortMode::Ascending };
		
		return SAssignNew(PropertyContent, SWidgetSwitcher)
			// Make sure the slots are coherent with the order of EReplicatedPropertyContent!
			.WidgetIndex(static_cast<int32>(EReplicatedPropertyContent::NoSelection))
			
			// EReplicatedPropertyContent::Properties
			+SWidgetSwitcher::Slot()
			[
				SAssignNew(ReplicatedProperties, SPropertyTreeView)
				.RootItemsSource(&RootPropertyRowData)
				.OnGetChildren(this, &SReplicatedPropertyView::GetPropertyRowChildren)
				.Columns(Columns)
				.ExpandableColumnLabel(ReplicationColumns::Property::LabelColumnId)
				.PrimarySort(PrimarySort)
				.SecondarySort(SecondarySort)
				.SelectionMode(ESelectionMode::Multi)
				.LeftOfSearchBar()
				[
					InArgs._LeftOfPropertySearchBar.Widget
				]
				.SelectedObjects(this, &SReplicatedPropertyView::GetObjectsSelectedForPropertyEditing)
			]
			
			// EReplicatedPropertyContent::NoSelection
			+SWidgetSwitcher::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoPropertyEditedObjects", "Select an object to see selected properties"))
			]
			
			// EReplicatedPropertyContent::SelectionTooBig
			+SWidgetSwitcher::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectionTooBig", "Select objects of the same type type to see selected properties"))
			];
	}
	
	TSharedRef<FReplicatedPropertyData> SReplicatedPropertyView::AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain)
	{
		return MakeShared<FReplicatedPropertyData>(MoveTemp(OwningClass), MoveTemp(PropertyChain));
	}

	void SReplicatedPropertyView::BuildRootPropertyRowData()
	{
		RootPropertyRowData.Empty(PropertyRowData.Num());
		for (const TSharedPtr<FReplicatedPropertyData>& PropertyData : PropertyRowData)
		{
			if (PropertyData->GetProperty().IsRootProperty())
			{
				RootPropertyRowData.Emplace(PropertyData);
			}
		}
	}
	
	TOptional<FSoftClassPath> SReplicatedPropertyView::GetClassForPropertiesFromSelection(const TArray<FSoftObjectPath>& Objects) const
	{
		const FSoftClassPath Class = PropertiesModel->GetObjectClass(Objects[0]);
		const bool bAllHaveSameClass = Algo::AllOf(Objects, [this, Class](const FSoftObjectPath& Object)
		{
			return PropertiesModel->GetObjectClass(Object) == Class;
		});
		return bAllHaveSameClass ? Class : TOptional<FSoftClassPath>{};
	}

	void SReplicatedPropertyView::GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild)
	{
		TArray<TSharedPtr<FReplicatedPropertyData>> Children;
		
		// Not the most efficient but it should be fine.
		for (const TSharedPtr<FReplicatedPropertyData>& Data : PropertyRowData)
		{
			if (Data->GetProperty().IsDirectChildOf(ReplicatedPropertyData->GetProperty()))
			{
				Children.Add(Data);
			}
		}

		Algo::ForEach(Children, [&ProcessChild](const TSharedPtr<FReplicatedPropertyData>& Data){ ProcessChild(Data); });
	}

	void SReplicatedPropertyView::SetPropertyContent(EReplicatedPropertyContent Content) const
	{
		PropertyContent->SetActiveWidgetIndex(static_cast<int32>(Content));
	}
}

#undef LOCTEXT_NAMESPACE