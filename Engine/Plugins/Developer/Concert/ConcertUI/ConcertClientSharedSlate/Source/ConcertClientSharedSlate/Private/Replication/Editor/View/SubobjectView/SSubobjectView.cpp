// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSubobjectView.h"

#include "Misc/EBreakBehavior.h"
#include "ReplicatedSubobjectData.h"
#include "Replication/Editor/Model/Subobject/ISubobjectModel.h"
#include "Replication/Editor/View/ObjectViewer/Tree/SelectionViewerColumns.h"

#include "Containers/Queue.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"

namespace UE::ConcertClientSharedSlate
{
	void SSubobjectView::Construct(const FArguments& InArgs, TSharedRef<ISubobjectModel> InSubobjectModel)
	{
		SubobjectModel = MoveTemp(InSubobjectModel);
		
		const auto TransformOp = [](const FReplicatedSubobjectData Data) -> FReplicatedObjectData
		{
			return ensureMsgf(!Data.IsSeparator(), TEXT("The widgets are supposed to be set up so that separators are not passed down to API users."))
				? *Data.GetObjectData() : FReplicatedObjectData{{}};
		};
		TArray<TReplicationColumn<FReplicatedSubobjectData>> Columns
		{
			ReplicationColumns::Subobject::DisplayColumn(*SubobjectModel).TransformColumn<FReplicatedSubobjectData>(TransformOp)
		};
		for (const ReplicationColumns::FReplicationSubobjectObjectColumn& Column : InArgs._AdditionalColumns)
		{
			Columns.Emplace(Column.TransformColumn<FReplicatedSubobjectData>(TransformOp));
		}
		
		ChildSlot
		[
			SAssignNew(SubobjectTreeView, SReplicationTreeView<FReplicatedSubobjectData>)
			.RootItemsSource(&RootSubobjectData)
			.OnGetChildren(this, &SSubobjectView::GetObjectRowChildren)
			.OnSelectionChanged_Lambda([this]()
			{
				OnSelectionChangedDelegate.Broadcast();
			})
			.OverrideColumnWidget(this, &SSubobjectView::GetOptionalColumnOverrideWidget)
			.IsSearchableItem_Lambda([](const TSharedPtr<FReplicatedSubobjectData>& Item){ return !Item->IsSeparator(); })
			.Columns(Columns)
			.ExpandableColumnLabel(ReplicationColumns::Subobject::DisplayColumnId)
			.HeaderRowVisibility(EVisibility::Collapsed)
			.SelectionMode(ESelectionMode::Multi)
		];

		SubobjectModel->OnHierarchyChanged().AddSP(this, &SSubobjectView::RefreshView);
		RefreshView();
	}
	
	void SSubobjectView::SetTopLevelObjects(const TArray<FSoftObjectPath>& InTopLevelObjects)
	{
		// TODO DP: Handle multiple top level objects. Create a new widget class that wraps SSubobjectView and displays a message.
		if (InTopLevelObjects.IsEmpty())
		{
			SubobjectModel->SetTopLevelObject(nullptr);
		}
		else
		{
			// This triggers OnHierarchyChanged which calls RefreshView 
			SubobjectModel->SetTopLevelObject(InTopLevelObjects[0]);
			
			// There might be nothing selected
			if (!AllSubobjectData.IsEmpty())
			{
				SubobjectTreeView->SetExpandedItems(AllSubobjectData, true);
			}
		}
	}

	void SSubobjectView::SelectTopLevelObjects()
	{
		if (SubobjectModel->GetTopLevelObject().IsNull())
		{
			return;
		}
		
		const TSharedPtr<FReplicatedSubobjectData>* TopLevelItem = RootSubobjectData.FindByPredicate([this](const TSharedPtr<FReplicatedSubobjectData>& RowData)
		{
			return !RowData->IsSeparator() && RowData->GetObjectData()->GetObjectPath() == SubobjectModel->GetTopLevelObject();
		});
		if (ensureMsgf(TopLevelItem, TEXT("Top level item is supposed to be contained")))
		{
			SubobjectTreeView->SetSelectedItems({ *TopLevelItem }, true);
		}
	}

	TArray<FSoftObjectPath> SSubobjectView::GetSelectedObjects() const
	{
		TArray<FSoftObjectPath> Result;
		Algo::TransformIf(SubobjectTreeView->GetSelectedItems(), Result,
			[](const TSharedPtr<FReplicatedSubobjectData>& RowData)
			{
				return !RowData->IsSeparator();
			},
			[](const TSharedPtr<FReplicatedSubobjectData>& RowData)
			{
				return RowData->GetObjectData()->GetObjectPath();
			});
		return Result;
	}

	void SSubobjectView::RefreshView()
	{
		// We want to re-use TSharedPtr items so selection states are retained.
		// PathToObjectDataCache will keep the items alive until it is replaced below.
		RootSubobjectData.Empty();
		AllSubobjectData.Empty();

		if (SubobjectModel->GetTopLevelObject().IsNull())
		{
			PathToObjectDataCache.Reset();
			SubobjectTreeView->OnItemsChanged();
			return;
		}
		
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedSubobjectData>> NewPathToObjectDataCache;
		BuildRootObjectData(NewPathToObjectDataCache);
		BuildChildObjectData(NewPathToObjectDataCache);

		// Only refresh if something actually changed
		if (!PathToObjectDataCache.OrderIndependentCompareEqual(NewPathToObjectDataCache))
		{
			// This will destroy the old items
			PathToObjectDataCache = MoveTemp(NewPathToObjectDataCache);
			SubobjectTreeView->OnItemsChanged();
		}
	}

	void SSubobjectView::BuildRootObjectData(TMap<FSoftObjectPath, TSharedPtr<FReplicatedSubobjectData>> NewPathToObjectDataCache)
	{
		// Top level item is always first
		const FSoftObjectPath TopLevelObject = SubobjectModel->GetTopLevelObject();
		const TSharedPtr<FReplicatedSubobjectData>* ExistingTopLevelItem = PathToObjectDataCache.Find(TopLevelObject);
		const TSharedPtr<FReplicatedSubobjectData> TopLevelItem = ExistingTopLevelItem ? *ExistingTopLevelItem : MakeShared<FReplicatedSubobjectData>(TopLevelObject);
		RootSubobjectData.Add(TopLevelItem);
		AllSubobjectData.Add(TopLevelItem);
		NewPathToObjectDataCache.Add(TopLevelObject, TopLevelItem);
		
		for (const FName Category : SubobjectModel->GetCategories())
		{
			bool bAddedSeparator = false;
			SubobjectModel->ForEachRootSubobject(Category, [this, &NewPathToObjectDataCache, &bAddedSeparator](const FSoftObjectPath& Subobject) 
			{
				// Separators are between categories
				if (!bAddedSeparator)
				{
					RootSubobjectData.Emplace(MakeShared<FReplicatedSubobjectData>());
					bAddedSeparator = true;
				}

				const TSharedPtr<FReplicatedSubobjectData>* Existing = PathToObjectDataCache.Find(Subobject);
				const TSharedPtr<FReplicatedSubobjectData> Item = Existing ? *Existing : MakeShared<FReplicatedSubobjectData>(Subobject);
				RootSubobjectData.Add(Item);
				AllSubobjectData.Add(Item);
				NewPathToObjectDataCache.Add(Subobject, Item);
				return EBreakBehavior::Continue;
			});
		}
	}

	void SSubobjectView::BuildChildObjectData(TMap<FSoftObjectPath, TSharedPtr<FReplicatedSubobjectData>>& NewPathToObjectDataCache)
	{
		TQueue<FSoftObjectPath> PendingObjects;
		for (const TSharedPtr<FReplicatedSubobjectData>& RowData : RootSubobjectData)
		{
			if (!RowData->IsSeparator() && RowData->GetObjectData()->GetObjectPath() != SubobjectModel->GetTopLevelObject())
			{
				PendingObjects.Enqueue(RowData->GetObjectData()->GetObjectPath());
			}
		}

		// Subobjects form a tree hierarchy so no need for infinite loop detection
		FSoftObjectPath CurrentPath;
		while (PendingObjects.Dequeue(CurrentPath))
		{
			const TSharedPtr<FReplicatedSubobjectData>* Existing = PathToObjectDataCache.Find(CurrentPath);
			TSharedPtr<FReplicatedSubobjectData> Data = Existing ? *Existing : MakeShared<FReplicatedSubobjectData>(CurrentPath);
			NewPathToObjectDataCache.Add(CurrentPath, Data);
			AllSubobjectData.Add(Data);

			SubobjectModel->ForEachDirectChildSubobject(CurrentPath, [&PendingObjects](const FSoftObjectPath& ChildPath)
			{
				PendingObjects.Enqueue(ChildPath);
				return EBreakBehavior::Continue;
			});
		}
	}

	void SSubobjectView::GetObjectRowChildren(TSharedPtr<FReplicatedSubobjectData> Item, TFunctionRef<void(TSharedPtr<FReplicatedSubobjectData>)> Callback) const
	{
		if (Item->IsSeparator()
			|| Item->GetObjectData()->GetObjectPath() == SubobjectModel->GetTopLevelObject())
		{
			return;
		}
		
		SubobjectModel->ForEachDirectChildSubobject(Item->GetObjectData()->GetObjectPath(), [this, &Callback](const FSoftObjectPath& Object)
		{
			if (const TSharedPtr<FReplicatedSubobjectData>* FoundItem = PathToObjectDataCache.Find(Object)
				; ensureMsgf(FoundItem, TEXT("No item created for %s"), *Object.ToString()))
			{
				Callback(*FoundItem);
			}
			return EBreakBehavior::Continue;
		});
	}

	TSharedPtr<SWidget> SSubobjectView::GetOptionalColumnOverrideWidget(const FName& Name, const FReplicatedSubobjectData& RowData) const
	{
		if (RowData.IsSeparator())
		{
			return SNew(SBox)
				[
					SNew(SSeparator)
					.SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
					.Thickness(1.f)
				 ];
		}
		return nullptr;
	}
}
