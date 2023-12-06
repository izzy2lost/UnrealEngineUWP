// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationStreamViewer.h"

#include "ConcertFrontendUtils.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/ISubobjectModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/ObjectViewer/Property/SPropertyTreeView.h"
#include "Replication/Editor/View/Tree/SelectionViewerColumns.h"
#include "SReplicatedPropertyView.h"
#include "Replication/Editor/View/ObjectUtils.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SObjectToPropertyView"

namespace UE::ConcertClientSharedSlate
{
	void SReplicationStreamViewer::Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel)
	{
		PropertiesModel = MoveTemp(InPropertiesModel);
		SubobjectModel = InArgs._SubobjectModel;
		
		ChildSlot
		[
			CreateContentWidget(InArgs)
		];

		Refresh();
		PropertyArea->SetExpanded(true);
	}

	void SReplicationStreamViewer::Refresh()
	{
		RefreshObjectData();
		RefreshPropertyData();
	}

	void SReplicationStreamViewer::RequestObjectColumnResort(const FName& ColumnId)
	{
		ReplicatedObjects->RequestResortForColumn(ColumnId);
	}

	void SReplicationStreamViewer::RequestPropertyColumnResort(const FName& ColumnId)
	{
		PropertySection->RequestResortForColumn(ColumnId);
	}

	TArray<FSoftObjectPath> SReplicationStreamViewer::GetObjectsBeingPropertyEdited() const
	{
		return PropertySection->GetObjectsSelectedForPropertyEditing();
	}

	void SReplicationStreamViewer::RefreshObjectData()
	{
		// Re-using existing instances is tricky: we cannot update the object path in an item because the list view will no detect this change;
		// list view only looks at the shared ptr address. So the UI will not be refreshed. Since the number of items will be small, just reallocate... 
		AllObjectRowData.Empty();

		// Try to re-use old instances by using the old PathToObjectDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>> NewPathToObjectDataCache;
		
		// Do a complete refresh.
		// Complete refresh is acceptable because the list is updated infrequently and typically small < 500 items.
		// An alternative would be to change RefreshObjectData to be called with two variables ObjectsAdded and ObjectsRemoved.
		PropertiesModel->ForEachReplicatedObject([this, &NewPathToObjectDataCache](const FSoftObjectPath& Path) mutable
		{
			const TSharedPtr<FReplicatedObjectData>* ExistingItem = PathToObjectDataCache.Find(Path);
			ExistingItem = ExistingItem ? ExistingItem : NewPathToObjectDataCache.Find(Path);
			const TSharedRef<FReplicatedObjectData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocateObjectData(Path);
			AllObjectRowData.AddUnique(Item);
			NewPathToObjectDataCache.Emplace(Path, Item);
			
			BuildObjectHierarchyIfNeeded(Item, NewPathToObjectDataCache);
			return EBreakBehavior::Continue;
		});

		// Only refresh the tree if it is necessary as it causes us to select stuff in the subobject view
		if (!PathToObjectDataCache.OrderIndependentCompareEqual(NewPathToObjectDataCache))
		{
			// If an item was removed, then NewPathToObjectDataCache does not contain it. 
			PathToObjectDataCache = MoveTemp(NewPathToObjectDataCache);

			// The tree view requires the item source to only contain the root items. Children are discovered via GetObjectRowChildren. We re-use GetObjectRowChildren to remove any non-root nodes.
			BuildRootObjectRowData();
			ReplicatedObjects->OnItemsChanged();
		}
	}

	void SReplicationStreamViewer::RefreshPropertyData()
	{
		PropertySection->RefreshPropertyData();
	}

	void SReplicationStreamViewer::SelectTopLevelObjects(TConstArrayView<FSoftObjectPath> Objects)
	{
		TArray<TSharedPtr<FReplicatedObjectData>> NewSelectedItems; 
		Algo::TransformIf(AllObjectRowData, NewSelectedItems, [&Objects](const TSharedPtr<FReplicatedObjectData>& ObjectData)
			{
				return Objects.Contains(ObjectData->GetObjectPath());
			},
			[](const TSharedPtr<FReplicatedObjectData>& ObjectData){ return ObjectData; }
		);
		if (!NewSelectedItems.IsEmpty())
		{
			ReplicatedObjects->SetSelectedItems(NewSelectedItems, true);
		}
	}

	void SReplicationStreamViewer::ExpandObjects(TConstArrayView<FSoftObjectPath> Objects, bool bRecursive)
	{
		if (Objects.IsEmpty())
		{
			return;
		}
		
		TArray<TSharedPtr<FReplicatedObjectData>> ItemsToExpand;
		ItemsToExpand.Reserve(Objects.Num());
		for (const FSoftObjectPath& Path : Objects)
		{
			if (const TSharedPtr<FReplicatedObjectData>* Item = PathToObjectDataCache.Find(Path))
			{
				ItemsToExpand.Add(*Item);
			}
			
			if (bRecursive && SubobjectModel)
			{
				SubobjectModel->ForEachSubobject([this, &ItemsToExpand](const FSoftObjectPath&, const FSoftObjectPath& ChildObject)
				{
					if (const TSharedPtr<FReplicatedObjectData>* Item = PathToObjectDataCache.Find(ChildObject))
					{
						ItemsToExpand.Add(*Item);
					}
					return EBreakBehavior::Continue;
				});
			}
		}

		if (ItemsToExpand.IsEmpty())
		{
			ReplicatedObjects->SetSelectedItems(ItemsToExpand, true);
		}
	}
	
	TSharedRef<FReplicatedObjectData> SReplicationStreamViewer::AllocateObjectData(FSoftObjectPath ObjectPath)
	{
		return MakeShared<FReplicatedObjectData>(MoveTemp(ObjectPath));
	}

	TSharedRef<SWidget> SReplicationStreamViewer::CreateContentWidget(const FArguments& InArgs)
	{
		return SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+SSplitter::Slot()
			 .Value(1.f)
			[
				CreateOutlinerSection(InArgs)
			]

			+SSplitter::Slot()
			.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SReplicationStreamViewer::GetPropertyAreaSizeRule))
			.Value(2.f)
			[
				CreatePropertiesSection(InArgs)
			];
	}

	TSharedRef<SWidget> SReplicationStreamViewer::CreateOutlinerSection(const FArguments& InArgs)
	{
		TArray Columns
		{
			ReplicationColumns::TopLevel::LabelColumn(PropertiesModel.ToSharedRef(), SubobjectModel.Get()),
			ReplicationColumns::TopLevel::TypeColumn(PropertiesModel.ToSharedRef())
		};
		Columns.Append(InArgs._AdditionalObjectColumns);
		
		const bool bHasNoOutlinerObjectsAttribute = InArgs._NoOutlinerObjects.IsBound() || InArgs._NoOutlinerObjects.IsSet(); 
		const TAttribute<FText> NoObjectsAttribute = bHasNoOutlinerObjectsAttribute ? InArgs._NoOutlinerObjects : LOCTEXT("NoObjects", "No objects to display");

		// Set both primary and secondary in case one is overriden but always use the override.
		const FColumnSortInfo PrimaryObjectSort = InArgs._PrimaryObjectSort.IsValid()
			? InArgs._PrimaryObjectSort
			: FColumnSortInfo{ ReplicationColumns::TopLevel::LabelColumnId, EColumnSortMode::Ascending };
		const FColumnSortInfo SecondaryObjectSort = InArgs._SecondaryObjectSort.IsValid()
			? InArgs._SecondaryObjectSort
			: FColumnSortInfo{ ReplicationColumns::TopLevel::LabelColumnId, EColumnSortMode::Ascending };
		
		return SAssignNew(ReplicatedObjects, SReplicationTreeView<FReplicatedObjectData>)
			.RootItemsSource(&RootObjectRowData)
			.OnGetChildren(this, &SReplicationStreamViewer::GetObjectRowChildren)
			.OnContextMenuOpening(InArgs._OnObjectsContextMenuOpening)
			.OnDeleteItems(InArgs._OnDeleteObjects).OnSelectionChanged_Lambda([this]()
			{
				RefreshPropertyData();
			})
			.Columns(Columns)
			.ExpandableColumnLabel(ReplicationColumns::TopLevel::LabelColumnId)
			.PrimarySort(PrimaryObjectSort)
			.SecondarySort(SecondaryObjectSort)
			.SelectionMode(ESelectionMode::Multi)
			.LeftOfSearchBar() [ InArgs._LeftOfObjectSearchBar.Widget ]
			.NoItemsContent() [ SNew(STextBlock).Text(NoObjectsAttribute) ];
	}

	TSharedRef<SWidget> SReplicationStreamViewer::CreatePropertiesSection(const FArguments& InArgs)
	{
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
				.OnAreaExpansionChanged(this, &SReplicationStreamViewer::OnPropertyAreaExpansionChanged)
				.Padding(0.0f)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplicatedProperties", "Subobjects & Properties"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				.BodyContent()
				[
					SAssignNew(PropertySection, SReplicatedPropertyView, PropertiesModel.ToSharedRef())
					.AdditionalPropertyColumns(InArgs._AdditionalPropertyColumns)
					.PrimarySort(InArgs._PrimaryPropertySort)
					.SecondarySort(InArgs._SecondaryPropertySort)
					.GetSelectedRootObjects_Lambda([this](){ return GetSelectedOutlinerObjects(); })
					.LeftOfPropertySearchBar()
					[
						InArgs._LeftOfPropertySearchBar.Widget
					]
				]
			];
	}

	void SReplicationStreamViewer::BuildRootObjectRowData()
	{
		TSet<TSharedPtr<FReplicatedObjectData>> NonRootNodes;
		for (const TSharedPtr<FReplicatedObjectData>& Node : AllObjectRowData)
		{
			GetObjectRowChildren(Node, [&NonRootNodes](TSharedPtr<FReplicatedObjectData> Child)
			{
				NonRootNodes.Add(MoveTemp(Child));
			});
		}

		// Make RootObjectRowData only contain those nodes which were not listed as children 
		RootObjectRowData.Empty(NonRootNodes.Num());
		for (const TSharedPtr<FReplicatedObjectData>& Node : AllObjectRowData)
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

	void SReplicationStreamViewer::BuildObjectHierarchyIfNeeded(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>>& NewPathToObjectDataCache)
	{
		// We're are supposed to display any hierarchy in the outliner if SubobjectModel is not set.
		const FSoftObjectPath& ObjectPath = ReplicatedObjectData->GetObjectPath();
		if (!SubobjectModel)
		{
			return;
		}

		// Find top level object of ReplicatedObjectData
		const TOptional<FSoftObjectPath> OwningActor = ObjectUtils::GetActorOf(ObjectPath);
		if (!SubobjectModel->IsTopLevelObject(ObjectPath) && !OwningActor)
		{
			return;
		}
		const FSoftObjectPath TopLevelObject = OwningActor.Get(ObjectPath);
		SubobjectModel->SetTopLevelObject(TopLevelObject);
		
		const auto AddItem = [this, &NewPathToObjectDataCache](const FSoftObjectPath& ObjectPath)
		{
			const TSharedPtr<FReplicatedObjectData>* ExistingItem = PathToObjectDataCache.Find(ObjectPath);
			ExistingItem = ExistingItem ? ExistingItem : NewPathToObjectDataCache.Find(ObjectPath);
			const TSharedRef<FReplicatedObjectData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocateObjectData(ObjectPath);
			AllObjectRowData.AddUnique(Item);
			NewPathToObjectDataCache.Emplace(ObjectPath, Item);
		};
		// Add all objects that appear in the hierarchy of ReplicatedObjectData
		AddItem(TopLevelObject);
		SubobjectModel->ForEachSubobject([this, &AddItem](const FSoftObjectPath&, const FSoftObjectPath& ChildObject)
		{
			 AddItem(ChildObject);
			return EBreakBehavior::Continue;
		});
	}

	void SReplicationStreamViewer::GetObjectRowChildren(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild)
	{
		// Important: this view should be possible to be built in programs, so it should not reference things like AActor, UActorComponent, ResolveObject, etc. directly.
		
		const FSoftObjectPath& SearchedObject = ReplicatedObjectData->GetObjectPath();
		if (!SubobjectModel)
		{
			return;
		}

		const TOptional<FSoftObjectPath> OwningActor = ObjectUtils::GetActorOf(SearchedObject);
		if (!SubobjectModel->IsTopLevelObject(SearchedObject) && !OwningActor)
		{
			return;
		}
		SubobjectModel->SetTopLevelObject(OwningActor.Get(SearchedObject));
		
		if (SubobjectModel->IsTopLevelObject(SearchedObject))
		{
			for (FName Category : SubobjectModel->GetCategories())
			{
				SubobjectModel->ForEachRootSubobject(Category, [this, &ProcessChild](const FSoftObjectPath& ChildObject)
				{
					if (const TSharedPtr<FReplicatedObjectData>* ObjectData = PathToObjectDataCache.Find(ChildObject))
					{
						ProcessChild(*ObjectData);
					}
					return EBreakBehavior::Continue;
				});
			}
		}
		else
		{
			SubobjectModel->ForEachDirectChildSubobject(SearchedObject, [this, &ProcessChild](const FSoftObjectPath& ChildObject)
			{
				if (const TSharedPtr<FReplicatedObjectData>* ObjectData = PathToObjectDataCache.Find(ChildObject))
				{
					ProcessChild(*ObjectData);
				}
				return EBreakBehavior::Continue;
			});
		}
	}
}

#undef LOCTEXT_NAMESPACE