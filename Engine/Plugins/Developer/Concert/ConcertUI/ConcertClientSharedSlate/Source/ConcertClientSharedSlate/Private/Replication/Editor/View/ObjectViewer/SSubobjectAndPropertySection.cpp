// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSubobjectAndPropertySection.h"

#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/View/IReplicationSubobjectView.h"
#include "Replication/Editor/View/ObjectViewer/Property/SReplicatedPropertiesView.h"
#include "Replication/Editor/View/ObjectViewer/Tree/SelectionViewerColumns.h"

#include "Algo/AllOf.h"
#include "Algo/ForEach.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSubobjectAndPropertiesSection"

namespace UE::ConcertClientSharedSlate
{
	void SSubobjectAndPropertySection::Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel)
	{
		PropertiesModel = MoveTemp(InPropertiesModel);
		SortPropertyRowPredicate = InArgs._SortPropertyRowPredicate;
		GetSelectedRootObjectsDelegate = InArgs._GetSelectedRootObjects;
		check(GetSelectedRootObjectsDelegate.IsBound());
		
		ChildSlot
		[
			CreateSubobjectsAndPropertiesSection(InArgs)
		];
	}

	void SSubobjectAndPropertySection::SelectRootObjects() const
	{
		if (SubobjectView)
		{
			SubobjectView->SelectTopLevelObjects();
		}
	}

	void SSubobjectAndPropertySection::ClearSubobjectSelection() const
	{
		if (SubobjectView)
		{
			SubobjectView->ClearRootObjects();
		}
	}

	void SSubobjectAndPropertySection::RefreshSubobjectData()
	{
		if (!SubobjectView)
		{
			return;
		}
		
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = GetSelectedRootObjectsDelegate.Execute();
		TArray<FSoftObjectPath> SelectedObjectPaths;
		Algo::Transform(SelectedObjects, SelectedObjectPaths, [](const TSharedPtr<FReplicatedObjectData>& Item){ return Item->GetObjectPath(); });
		SubobjectView->SetTopLevelObjects(SelectedObjectPaths);
	}
	
	void SSubobjectAndPropertySection::RefreshPropertyData()
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

	TArray<FSoftObjectPath> SSubobjectAndPropertySection::GetObjectsSelectedForPropertyEditing() const
	{
		if (SubobjectView)
		{
			return SubobjectView->GetSelectedObjects();
		}

		TArray<FSoftObjectPath> Result;
		Algo::Transform(GetSelectedRootObjectsDelegate.Execute(), Result, [](const TSharedPtr<FReplicatedObjectData>& ObjectData)
		{
			return ObjectData->GetObjectPath();
		});
		return Result;
	}

	TSharedRef<SWidget> SSubobjectAndPropertySection::CreateSubobjectsAndPropertiesSection(const FArguments& InArgs)
	{
		if (InArgs._SubobjectView.IsValid())
		{
			SubobjectView = InArgs._SubobjectView;
			SubobjectView->OnSelectionChanged().AddSP(this, &SSubobjectAndPropertySection::OnSubobjectSelectionChanged);
			return SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]()
				{
					return GetSelectedRootObjectsDelegate.Execute().IsEmpty() ? 1 : 0;
				})
			
				+SWidgetSwitcher::Slot()
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)

					+SSplitter::Slot()
					.Value(1.f)
					[
						SubobjectView.ToSharedRef()
					]
					
					+SSplitter::Slot()
					.Value(2.f)
					[
						CreatePropertiesView(InArgs)
					]
				]
			
				+SWidgetSwitcher::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoRootObjects", "Select an object to see selected properties"))
				];
		}
		
		return CreatePropertiesView(InArgs);
	}

	TSharedRef<SWidget> SSubobjectAndPropertySection::CreatePropertiesView(const FArguments& InArgs)
	{
		TArray Columns
		{
			ReplicationColumns::Property::LabelColumn(),
			ReplicationColumns::Property::TypeColumn()
		};
		Columns.Append(InArgs._AdditionalPropertyColumns);
		
		return SAssignNew(PropertyContent, SWidgetSwitcher)
			// Make sure the slots are coherent with the order of EReplicatedPropertyContent!
			.WidgetIndex(static_cast<int32>(EReplicatedPropertyContent::NoSelection))
			
			// EReplicatedPropertyContent::Properties
			+SWidgetSwitcher::Slot()
			[
				SAssignNew(ReplicatedProperties, SReplicatedPropertiesView)
				.RootItemsSource(&RootPropertyRowData)
				.OnGetChildren(this, &SSubobjectAndPropertySection::GetPropertyRowChildren)
				.Columns(Columns)
				.ExpandableColumnLabel(ReplicationColumns::Property::LabelColumnId)
				.SelectionMode(ESelectionMode::Multi)
				.LeftOfSearchBar()
				[
					InArgs._LeftOfPropertySearchBar.Widget
				]
				.SelectedObjects(this, &SSubobjectAndPropertySection::GetObjectsSelectedForPropertyEditing)
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
	
	TSharedRef<FReplicatedPropertyData> SSubobjectAndPropertySection::AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain)
	{
		return MakeShared<FReplicatedPropertyData>(MoveTemp(OwningClass), MoveTemp(PropertyChain));
	}

	void SSubobjectAndPropertySection::BuildRootPropertyRowData()
	{
		RootPropertyRowData.Empty(PropertyRowData.Num());
		for (const TSharedPtr<FReplicatedPropertyData>& PropertyData : PropertyRowData)
		{
			if (PropertyData->GetProperty().IsRootProperty())
			{
				RootPropertyRowData.Emplace(PropertyData);
			}
		}

		SortPropertyRowArray(RootPropertyRowData);
	}
	
	TOptional<FSoftClassPath> SSubobjectAndPropertySection::GetClassForPropertiesFromSelection(const TArray<FSoftObjectPath>& Objects) const
	{
		const FSoftClassPath Class = PropertiesModel->GetObjectClass(Objects[0]);
		const bool bAllHaveSameClass = Algo::AllOf(Objects, [this, Class](const FSoftObjectPath& Object)
		{
			return PropertiesModel->GetObjectClass(Object) == Class;
		});
		return bAllHaveSameClass ? Class : TOptional<FSoftClassPath>{};
	}

	void SSubobjectAndPropertySection::GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild)
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

		SortPropertyRowArray(Children);
		Algo::ForEach(Children, [&ProcessChild](const TSharedPtr<FReplicatedPropertyData>& Data){ ProcessChild(Data); });
	}

	void SSubobjectAndPropertySection::OnSubobjectSelectionChanged()
	{
		RefreshPropertyData();
	}

	void SSubobjectAndPropertySection::SortPropertyRowArray(TArray<TSharedPtr<FReplicatedPropertyData>>& ToSort) const
	{
		if (SortPropertyRowPredicate.IsBound())
		{
			ToSort.Sort([this](const TSharedPtr<FReplicatedPropertyData>& Left, const TSharedPtr<FReplicatedPropertyData>& Right)
			{
				return SortPropertyRowPredicate.Execute(*Left.Get(), *Right.Get());
			});
		}
	}

	void SSubobjectAndPropertySection::SetPropertyContent(EReplicatedPropertyContent Content) const
	{
		PropertyContent->SetActiveWidgetIndex(static_cast<int32>(Content));
	}
}

#undef LOCTEXT_NAMESPACE