// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyReplicationSelectionViewer.h"

#include "ConcertFrontendUtils.h"
#include "StreamEditor/Model/IObjectToPropertiesModel.h"
#include "ReplicatedObjectData.h"
#include "ReplicatedPropertyData.h"
#include "SelectionViewerColumns.h"

#include "Algo/AllOf.h"
#include "Algo/ForEach.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPropertyReplicationSelectionViewer"

namespace UE::MultiUserReplicationEditor
{
	void SPropertyReplicationSelectionViewer::Construct(const FArguments& InArgs, TSharedRef<IObjectToPropertiesModel> InPropertiesModel)
	{
		PropertiesModel = MoveTemp(InPropertiesModel);

		SortPropertyRowPredicate = InArgs._SortPropertyRowPredicate;
		
		ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				// Actors
				+SSplitter::Slot()
				.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SPropertyReplicationSelectionViewer::GetActorAreaSizeRule))
				.Value(0.55f)
				[
					CreateActorsSection(InArgs)
				]
				
				// Properties
				+SSplitter::Slot()
				.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SPropertyReplicationSelectionViewer::GetPropertyAreaSizeRule))
				.Value(0.45f)
				[
					CreatePropertiesSection(InArgs)
				]
			]
		];

		RefreshObjectData();
		RefreshPropertyData();

		ActorArea->SetExpanded(true);
		PropertyArea->SetExpanded(true);
	}

	void SPropertyReplicationSelectionViewer::RefreshObjectData()
	{
		const int32 NumElements = PropertiesModel->GetNumReplicatedObjects();
		// Re-using existing instances is tricky: we cannot update the object path in an item because the list view will no detect this change;
		// list view only looks at the shared ptr address. So the UI will not be refreshed. Since the number of items will be small, just reallocate... 
		ObjectRowData.Empty(NumElements);

		// Try to re-use old instances by using the old PathToObjectDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>> NewPathToObjectDataCache;
		
		// Do a complete refresh.
		// Complete refresh is acceptable because the list is updated infrequently and typically small < 500 items.
		// An alternative would be to change RefreshObjectData to be called with two variables ObjectsAdded and ObjectsRemoved.
		PropertiesModel->ForEachReplicatedObject([this, &NewPathToObjectDataCache](const FSoftObjectPath& Path) mutable
		{
			const TSharedPtr<FReplicatedObjectData>* ExistingItem = PathToObjectDataCache.Find(Path);
			const TSharedRef<FReplicatedObjectData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocateObjectData(Path);
			ObjectRowData.Emplace(Item);
			NewPathToObjectDataCache.Emplace(Path, Item);
			return EBreakBehavior::Continue;
		});
		
		// If an item was removed, then NewPathToObjectDataCache does not contain it. 
		PathToObjectDataCache = MoveTemp(NewPathToObjectDataCache);

		// The tree view requires the item source to only contain the root items. Children are discovered via GetObjectRowChildren. We re-use GetObjectRowChildren to remove any non-root nodes.
		BuildRootObjectRowData();
		ReplicatedObjects->OnItemsChanged();
	}

	void SPropertyReplicationSelectionViewer::RefreshPropertyData()
	{
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = ReplicatedObjects->GetSelectedItems();
		if (SelectedObjects.IsEmpty())
		{
			SetPropertyContent(EReplicatedPropertyContent::NoSelection);
			return;
		}

		// Technically, the classes just need to be compatible with each other... but it is easier to just allow the same class.
		const TOptional<FSoftClassPath> SharedClass = GetClassForPropertiesFromSelection();
		if (!SharedClass)
		{
			SetPropertyContent(EReplicatedPropertyContent::SelectionTooBig);
			return;
		}
		const FSoftClassPath Class = *SharedClass;

		// Build the set of properties that are shared by all of the selected objects
		TSet<FConcertPropertyChain> SharedProperties = PropertiesModel->GetAllProperties(SelectedObjects[0]->GetObjectPath());
		for (int32 i = 1; i < SelectedObjects.Num(); ++i)
		{
			SharedProperties = PropertiesModel->GetAllProperties(SelectedObjects[i]->GetObjectPath()).Union(SharedProperties);
		}
		
		// Try to re-use old instances by using the old PathToObjectDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> NewPathToPropertyDataCache;
		
		PropertyRowData.Empty(PropertiesModel->GetNumProperties(SelectedObjects[0]->GetObjectPath()));
		for (const FConcertPropertyChain& PropertyChain : SharedProperties)
		{
			const TSharedPtr<FReplicatedPropertyData>* ExistingItem = ChainToPropertyDataCache.Find(PropertyChain);
			const TSharedRef<FReplicatedPropertyData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocatePropertyData(Class, PropertyChain);
			PropertyRowData.Emplace(Item);
			NewPathToPropertyDataCache.Emplace(PropertyChain, Item);
		}

		// If an item was removed, then NewPathToPropertyDataCache does not contain it. 
		ChainToPropertyDataCache = MoveTemp(NewPathToPropertyDataCache);
		
		// The tree view requires the item source to only contain the root items.
		BuildRootPropertyRowData();
		SetPropertyContent(EReplicatedPropertyContent::Properties);
		ReplicatedProperties->OnItemsChanged();
	}

	TOptional<FSoftClassPath> SPropertyReplicationSelectionViewer::GetClassForPropertiesFromSelection() const
	{
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = ReplicatedObjects->GetSelectedItems();
		const FSoftClassPath Class = PropertiesModel->GetObjectClass(SelectedObjects[0]->GetObjectPath());
		const bool bAllHaveSameClass = Algo::AllOf(SelectedObjects, [this, Class](const TSharedPtr<FReplicatedObjectData>& Object)
		{
			return PropertiesModel->GetObjectClass(Object->GetObjectPath()) == Class;
		});
		return bAllHaveSameClass ? Class : TOptional<FSoftClassPath>{};
	}

	FSoftObjectPath SPropertyReplicationSelectionViewer::GetParent(const FSoftObjectPath& Path) const
	{
		UObject* Object = Path.ResolveObject();
		if (!Object)
		{
			return nullptr;
		}

		for (UObject* Current = Object->GetOuter(); Current; Current = Current->GetOuter())
		{
			if (PathToObjectDataCache.Contains(Current))
			{
				return Current;
			}
		}

		return nullptr;
	}

	TSharedRef<FReplicatedObjectData> SPropertyReplicationSelectionViewer::AllocateObjectData(FSoftObjectPath ObjectPath)
	{
		return MakeShared<FReplicatedObjectData>(MoveTemp(ObjectPath));
	}

	TSharedRef<FReplicatedPropertyData> SPropertyReplicationSelectionViewer::AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain)
	{
		return MakeShared<FReplicatedPropertyData>(MoveTemp(OwningClass), MoveTemp(PropertyChain));
	}

	TSharedRef<SWidget> SPropertyReplicationSelectionViewer::CreateActorsSection(const FArguments& InArgs)
	{
		TArray<ReplicationObjectColumns::FReplicationObjectColumn> Columns
		{
			ReplicationObjectColumns::IconColumn(PropertiesModel.ToSharedRef()),
			ReplicationObjectColumns::LabelColumn(),
			ReplicationObjectColumns::TypeColumn(PropertiesModel.ToSharedRef())
		};
		Columns.Append(InArgs._AdditionalObjectColumns);
		
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				SAssignNew(ActorArea, SExpandableArea)
				.InitiallyCollapsed(true) 
				.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
				.BorderImage_Lambda([this]() { return ConcertFrontendUtils::GetExpandableAreaBorderImage(*ActorArea); })
				.BodyBorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.BodyBorderBackgroundColor(FLinearColor::White)
				.OnAreaExpansionChanged(this, &SPropertyReplicationSelectionViewer::OnActorAreaExpansionChanged)
				.Padding(0.0f)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplicatedObjects", "Replicated Objects"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				.BodyContent()
				[
					SAssignNew(ReplicatedObjects, SReplicationTreeView<TSharedPtr<FReplicatedObjectData>>)
					.RootItemsSource(&RootObjectRowData)
					.OnGetChildren(this, &SPropertyReplicationSelectionViewer::GetObjectRowChildren)
					.OnContextMenuOpening(InArgs._OnObjectsContextMenuOpening)
					.OnDeleteItems(InArgs._OnDeleteObjects)
					.OnSelectionChanged_Lambda([this](){ RefreshPropertyData(); })
					.Columns(Columns)
					.ExpandableColumnLabel(ReplicationObjectColumns::LabelColumnId)
					.SelectionMode(ESelectionMode::Multi)
					.LeftOfSearchBar()
					[
						InArgs._LeftOfObjectSearchBar.Widget
					]
				]
			];
	}

	TSharedRef<SWidget> SPropertyReplicationSelectionViewer::CreatePropertiesSection(const FArguments& InArgs)
	{
		TArray<ReplicationPropertyColumns::FReplicationPropertyColumn> Columns
		{
			ReplicationPropertyColumns::LabelColumn()
		};
		Columns.Append(InArgs._AdditionalPropertyColumns);
		
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				SAssignNew(PropertyArea, SExpandableArea)
				.InitiallyCollapsed(true) 
				.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
				.BorderImage_Lambda([this]() { return ConcertFrontendUtils::GetExpandableAreaBorderImage(*PropertyArea); })
				.BodyBorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.BodyBorderBackgroundColor(FLinearColor::White)
				.OnAreaExpansionChanged(this, &SPropertyReplicationSelectionViewer::OnPropertyAreaExpansionChanged)
				.Padding(0.0f)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplicatedProperties", "Replicated Properties"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				.BodyContent()
				[
					// Make sure the slots are coherent with the order of EReplicatedPropertyContent!
					SAssignNew(PropertyContent, SWidgetSwitcher)
					.WidgetIndex(static_cast<int32>(EReplicatedPropertyContent::NoSelection))
					
					// EReplicatedPropertyContent::Properties
					+SWidgetSwitcher::Slot()
					[
						SAssignNew(ReplicatedProperties, SReplicationTreeView<TSharedPtr<FReplicatedPropertyData>>)
						.RootItemsSource(&RootPropertyRowData)
						.OnGetChildren(this, &SPropertyReplicationSelectionViewer::GetPropertyRowChildren)
						.Columns(Columns)
						.ExpandableColumnLabel(ReplicationPropertyColumns::LabelColumnId)
						.SelectionMode(ESelectionMode::Multi)
						.LeftOfSearchBar()
						[
							InArgs._LeftOfPropertySearchBar.Widget
						]
					]
					
					// EReplicatedPropertyContent::NoSelection
					+SWidgetSwitcher::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoSelection", "Select an object to see selected properties"))
					]
					
					// EReplicatedPropertyContent::SelectionTooBig
					+SWidgetSwitcher::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectionTooBig", "Select objects of the same type type to see selected properties"))
					]
				]
			];
	}
	
	void SPropertyReplicationSelectionViewer::BuildRootObjectRowData()
	{
		TSet<TSharedPtr<FReplicatedObjectData>> NonRootNodes;
		for (const TSharedPtr<FReplicatedObjectData>& Node : ObjectRowData)
		{
			GetObjectRowChildren(Node, [&NonRootNodes](TSharedPtr<FReplicatedObjectData> Child)
			{
				NonRootNodes.Add(MoveTemp(Child));
			});
		}

		// Make RootObjectRowData only contains those nodes which were not listed as children 
		RootObjectRowData.Empty(NonRootNodes.Num());
		for (const TSharedPtr<FReplicatedObjectData>& Node : ObjectRowData)
		{
			if (!NonRootNodes.Contains(Node))
			{
				RootObjectRowData.Add(Node);
			}
		}
		
		RootObjectRowData.Sort([](const TSharedPtr<FReplicatedObjectData>& Left, const TSharedPtr<FReplicatedObjectData>& Right)
		{
			return Left->GetObjectPath().GetSubPathString() < Right->GetObjectPath().GetSubPathString();
		});
	}

	void SPropertyReplicationSelectionViewer::BuildRootPropertyRowData()
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

	void SPropertyReplicationSelectionViewer::GetObjectRowChildren(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild)
	{
		const UObject* ResolvedObject = ReplicatedObjectData->GetObjectPath().ResolveObject();
		const AActor* ResolvedActor = Cast<AActor>(ResolvedObject);

		GetActorComponentsAsChildRows(ResolvedActor, ProcessChild);
	}

	void SPropertyReplicationSelectionViewer::GetActorComponentsAsChildRows(const AActor* ResolvedActor, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild)
	{
		if (!ResolvedActor)
		{
			return;
		}

		for (UActorComponent* Component : ResolvedActor->GetComponents())
		{
			const FSoftObjectPath ComponentPath = Component;
			if (const TSharedPtr<FReplicatedObjectData>* ComponentChild = PathToObjectDataCache.Find(ComponentPath))
			{
				ProcessChild(*ComponentChild);
			}
		}
	}

	void SPropertyReplicationSelectionViewer::GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild)
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
	
	void SPropertyReplicationSelectionViewer::SortPropertyRowArray(TArray<TSharedPtr<FReplicatedPropertyData>>& ToSort) const
	{
		if (SortPropertyRowPredicate.IsBound())
		{
			ToSort.Sort([this](const TSharedPtr<FReplicatedPropertyData>& Left, const TSharedPtr<FReplicatedPropertyData>& Right)
			{
				return SortPropertyRowPredicate.Execute(Left, Right);
			});
		}
	}

	void SPropertyReplicationSelectionViewer::SetPropertyContent(EReplicatedPropertyContent Content) const
	{
		PropertyContent->SetActiveWidgetIndex(static_cast<int32>(Content));
	}
}

#undef LOCTEXT_NAMESPACE