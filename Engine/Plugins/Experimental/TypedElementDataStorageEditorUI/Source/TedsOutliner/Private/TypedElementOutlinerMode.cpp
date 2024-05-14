// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerMode.h"

#include "TypedElementOutlinerFilter.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "TypedElementOutlinerHierarchy.h"
#include "TypedElementOutlinerItem.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "FolderTreeItem.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "TEDSOutlinerMode"

namespace UE::TEDSOutliner::Local
{
	// Drag drop currently disabled as we are missing data marshalling for hierarchies from TEDS to the world
	static bool TEDSOutlinerDragDropEnabled = false;
	static FAutoConsoleVariableRef TEDSOutlinerDragDropEnabledCvar(TEXT("TEDS.UI.EnableTEDSOutlinerDragDrop"), TEDSOutlinerDragDropEnabled, TEXT("Enable drag/drop for the generic TEDS Outliner."));
	static FName ContextMenuName("TEDSOutlinerContextMenu");
}



FTypedElementOutlinerMode::FTypedElementOutlinerMode(const FTypedElementOutlinerModeParams& InParams)
	: ISceneOutlinerMode(InParams.SceneOutliner)
	, InitialQueryDescription(InParams.QueryDescription)
	, SelectionSetName(InParams.SelectionSetOverride)
	, bSelectionDirty(SelectionSetName.IsSet() ? true : false)
	, HierarchyData(InParams.HierarchyData)
{
	using namespace TypedElementQueryBuilder;

	if(SelectionSetName.IsSet())
	{
		// We currently need a ticker to update selection because TEDS Observers can be fired before the data in columns is init
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[this](float DeltaTimeInSeconds)
				{
					Tick();
					return true;
				}));
	}
	
	
	if(InParams.bUseDefaultTEDSFilters)
	{
		// Create separate categories for columns and tags
		TSharedRef<FFilterCategory> TEDSColumnFilterCategory = MakeShared<FFilterCategory>(LOCTEXT("TEDSColumnFilters", "TEDS Columns"), LOCTEXT("TEDSColumnFiltersTooltip", "Filter by TEDS columns"));
		TSharedRef<FFilterCategory> TEDSTagFilterCategory = MakeShared<FFilterCategory>(LOCTEXT("TEDSTagFilters", "TEDS Tags"), LOCTEXT("TEDSTagFiltersTooltip", "Filter by TEDS Tags"));

		const UStruct* TEDSColumn = FTypedElementDataStorageColumn::StaticStruct();
		const UStruct* TEDSStruct = FTypedElementDataStorageTag::StaticStruct();

		// Grab all UStruct types to see if they derive from FTypedElementDataStorageColumn or FTypedElementDataStorageTag
		ForEachObjectOfClass(UScriptStruct::StaticClass(), [&](UObject* Obj)
		{
			if (UScriptStruct* Struct = Cast<UScriptStruct>(Obj))
			{
				if(Struct->IsChildOf(TEDSColumn) || Struct->IsChildOf(TEDSStruct))
				{
					// Create an empty query desc
					TypedElementDataStorage::FQueryDescription FilterQueryDesc =
					Select()
					.Where()
						.All(Struct)
					.Compile();

					// Create the filter
					TSharedRef<FTEDSOutlinerFilter> TEDSFilter = MakeShared<FTEDSOutlinerFilter>(Struct->GetFName(), Struct->GetDisplayNameText(),
						Struct->IsChildOf(TEDSColumn) ? TEDSColumnFilterCategory : TEDSTagFilterCategory, this, FilterQueryDesc);
					SceneOutliner->AddFilterToFilterBar(TEDSFilter);
				}
			}
		});
	}

	// Custom filters input by the user
	TSharedRef<FFilterCategory> CustomFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("TEDSFilters", "TEDS Custom Filters"), LOCTEXT("TEDSFiltersTooltip", "Filter by custom TEDS queries"));

	for(const TPair<FName, const TypedElementDataStorage::FQueryDescription>& FilterQuery : InParams.FilterQueries)
	{
		// TEDS-Outliner TODO: Custom filters need a localizable display name instead of using the FName, but we need to change how they are added first
		// to see if it can be consolidated with the SFilterBar API
		TSharedRef<FTEDSOutlinerFilter> TEDSFilter = MakeShared<FTEDSOutlinerFilter>(FilterQuery.Key, FText::FromName(FilterQuery.Key), CustomFiltersCategory, this, FilterQuery.Value);
		SceneOutliner->AddFilterToFilterBar(TEDSFilter);
	
	}
}

FTypedElementOutlinerMode::~FTypedElementOutlinerMode()
{
	ClearSelection();
	UnregisterQueries();
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

void FTypedElementOutlinerMode::Rebuild()
{
	Hierarchy = CreateHierarchy();
}

void FTypedElementOutlinerMode::Tick()
{
	if(bSelectionDirty && SelectionSetName.IsSet())
	{
		bSelectionDirty = false;

		// The selection in TEDS was changed, update the outliner to respond
		SceneOutliner->SetSelection([this](ISceneOutlinerTreeItem& InItem) -> bool
		{
			if(FTypedElementOutlinerTreeItem* TEDSItem = InItem.CastTo<FTypedElementOutlinerTreeItem>())
			{
				TypedElementDataStorage::RowHandle RowHandle = TEDSItem->GetRowHandle();

				if(FTypedElementSelectionColumn* SelectionColumn = Storage->GetColumn<FTypedElementSelectionColumn>(RowHandle))
				{
					return SelectionColumn->SelectionSet == SelectionSetName;
				}
			}
			return false;
		});
	}
}

void FTypedElementOutlinerMode::ClearSelection()
{
	if(!SelectionSetName.IsSet())
	{
		return;
	}
	
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	TArray<TypedElementDataStorage::RowHandle> RowsToRemoveSelectionColumn;

	// Query to remove the selection column from all rows that belong to this selection set
	TypedElementDataStorage::DirectQueryCallback RowCollector = CreateDirectQueryCallbackBinding(
	[this, &RowsToRemoveSelectionColumn](DSI::IDirectQueryContext& Context)
	{
		TConstArrayView<TypedElementDataStorage::RowHandle> Rows = Context.GetRowHandles();

		for(const TypedElementDataStorage::RowHandle RowHandle : Rows)
		{
			if(const FTypedElementSelectionColumn* SelectionColumn = Storage->GetColumn<FTypedElementSelectionColumn>(RowHandle))
			{
				if(SelectionColumn->SelectionSet == SelectionSetName)
				{
					RowsToRemoveSelectionColumn.Add(RowHandle);
				}
			}
		}
	});

	Storage->RunQuery(SelectedRowsQuery, RowCollector);

	for(const TypedElementDataStorage::RowHandle RowHandle : RowsToRemoveSelectionColumn)
	{
		Storage->RemoveColumn<FTypedElementSelectionColumn>(RowHandle);
	}

}

void FTypedElementOutlinerMode::SynchronizeSelection()
{
	if(SelectionSetName.IsSet())
	{
		bSelectionDirty = true;
	}
	
}

void FTypedElementOutlinerMode::OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection)
{
	if(!SelectionSetName.IsSet())
	{
		return;
	}
	
	if(SelectionType == ESelectInfo::Direct)
	{
		return; // Direct selection means we selected from outside the Outliner i.e through TEDS, so we don't need to redo the column addition
	}
	
	ClearSelection();

	// The selection in the Outliner changed, update TEDS
	Selection.ForEachItem([this](FSceneOutlinerTreeItemPtr& Item)
	{
		if(FTypedElementOutlinerTreeItem* TEDSItem = Item->CastTo<FTypedElementOutlinerTreeItem>())
		{
			TypedElementDataStorage::RowHandle RowHandle = TEDSItem->GetRowHandle();

			Storage->AddColumn(RowHandle, FTypedElementSelectionColumn{ .SelectionSet = SelectionSetName.GetValue() });
		}
	});
}

TSharedPtr<FDragDropOperation> FTypedElementOutlinerMode::CreateDragDropOperation(const FPointerEvent& MouseEvent, const TArray<FSceneOutlinerTreeItemPtr>& InTreeItems) const
{
	// We don't want drag/drop if this TEDS Outliner isn't showing any hierarchy data
	if(!HierarchyData.IsSet())
	{
		return nullptr;
	}
	
	TArray<TypedElementDataStorage::RowHandle> DraggedRowHandles;

	for(const FSceneOutlinerTreeItemPtr& Item :InTreeItems)
	{
		const FTypedElementOutlinerTreeItem* TEDSItem = Item->CastTo<FTypedElementOutlinerTreeItem>();
		if(ensureMsgf(TEDSItem, TEXT("We should only have TEDS items in the TEDS Outliner")))
		{
			DraggedRowHandles.Add(TEDSItem->GetRowHandle());
		}
	}

	return FTEDSDragDropOp::New(DraggedRowHandles);
}

bool FTypedElementOutlinerMode::ParseDragDrop(FSceneOutlinerDragDropPayload& OutPayload, const FDragDropOperation& Operation) const
{
	if (Operation.IsOfType<FTEDSDragDropOp>())
	{
		const FTEDSDragDropOp& TEDSOp = static_cast<const FTEDSDragDropOp&>(Operation);

		for(TypedElementDataStorage::RowHandle RowHandle : TEDSOp.DraggedRows)
		{
			OutPayload.DraggedItems.Add(SceneOutliner->GetTreeItem(RowHandle));
		}
		return true;
	}
	return false;
}

FSceneOutlinerDragValidationInfo FTypedElementOutlinerMode::ValidateDrop(const ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload) const
{
	// We don't want drag/drop if this TEDS Outliner isn't showing any hierarchy data
	if(!HierarchyData.IsSet())
	{
		return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric,
			LOCTEXT("DropDisabled", "Drag/Drop is disabled due to missing hierarchy data!"));
	}
	
	TArray<TypedElementDataStorage::RowHandle> DraggedRowHandles;

	Payload.ForEachItem<FTypedElementOutlinerTreeItem>([&DraggedRowHandles](FTypedElementOutlinerTreeItem& TEDSItem)
		{
			DraggedRowHandles.Add(TEDSItem.GetRowHandle());
		});

	// Dropping onto another item
	// TEDS-Outliner TODO: Need better drag/drop validation and better place for this, TEDS-Outliner does not know about what types these rows are and all types that exist and what attachment is valid
	if(const FTypedElementOutlinerTreeItem* TEDSItem = DropTarget.CastTo<FTypedElementOutlinerTreeItem>())
	{
		TypedElementDataStorage::RowHandle DropTargetRowHandle = TEDSItem->GetRowHandle();

		FTypedElementClassTypeInfoColumn* DropTargetTypeInfoColumn = Storage->GetColumn<FTypedElementClassTypeInfoColumn>(DropTargetRowHandle);

		// For now only allow attachment to same type
		if(!DropTargetTypeInfoColumn || !DropTargetTypeInfoColumn->TypeInfo.Get())
		{
			return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, LOCTEXT("DropTargetInvalidType", "Invalid Drop target"));
		}

		
		// TEDS-Outliner TODO: Currently we detect parent changes by removing the column and then adding the column back with the new parent 
		for(TypedElementDataStorage::RowHandle RowHandle : DraggedRowHandles)
		{
			FTypedElementClassTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementClassTypeInfoColumn>(RowHandle);

			if(!TypeInfoColumn || !TypeInfoColumn->TypeInfo.Get())
			{
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, LOCTEXT("DragItemInvalidType", "Invalid Drag item"));
			}

			// TEDS-Outliner TOOD UE-205438: Proper drag/drop validation
			if(TypeInfoColumn->TypeInfo.Get() != DropTargetTypeInfoColumn->TypeInfo.Get())
			{
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, FText::Format(LOCTEXT("DragDropTypeMismatch", "Cannot drag a {0} into a {1}"), FText::FromName(TypeInfoColumn->TypeInfo.Get()->GetFName()), FText::FromName(DropTargetTypeInfoColumn->TypeInfo.Get()->GetFName())));
			}
		}

		return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::CompatibleAttach, LOCTEXT("ValidDrop", "Valid Drop"));
	}
	// Dropping onto root, remove parent
	else if (const FFolderTreeItem* FolderItem = DropTarget.CastTo<FFolderTreeItem>())
	{
		const FFolder DestinationPath = FolderItem->GetFolder();

		if(DestinationPath.IsNone())
		{
			bool bValidDetach = false;
			
			for(TypedElementDataStorage::RowHandle RowHandle : DraggedRowHandles)
			{
				if(Storage->HasColumns(RowHandle, MakeArrayView({HierarchyData.GetValue().HierarchyColumn})))
				{
					bValidDetach = true;
					break;
				}
			}

			if(bValidDetach)
			{
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::CompatibleDetach, LOCTEXT("MoveToRoot", "Move to root"));
			}
		}
	}

	return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, LOCTEXT("InvalidDrop", "Invalid Drop target"));
}

void FTypedElementOutlinerMode::OnDrop(ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload, const FSceneOutlinerDragValidationInfo& ValidationInfo) const
{
	if(!UE::TEDSOutliner::Local::TEDSOutlinerDragDropEnabledCvar->GetBool() || !HierarchyData.IsSet())
	{
		return;
	}
	
	TArray<TypedElementDataStorage::RowHandle> DraggedRowHandles;

	Payload.ForEachItem<FTypedElementOutlinerTreeItem>([&DraggedRowHandles](FTypedElementOutlinerTreeItem& TEDSItem)
	{
		DraggedRowHandles.Add(TEDSItem.GetRowHandle());
	});

	if(ValidationInfo.CompatibilityType == ESceneOutlinerDropCompatibility::CompatibleDetach)
	{
		for(TypedElementDataStorage::RowHandle RowHandle : DraggedRowHandles)
		{
			Storage->RemoveColumn(RowHandle, HierarchyData.GetValue().HierarchyColumn);
			Storage->AddColumn<FTypedElementSyncBackToWorldTag>(RowHandle);
		}
	}
	
	if(const FTypedElementOutlinerTreeItem* TEDSItem = DropTarget.CastTo<FTypedElementOutlinerTreeItem>())
	{
		TypedElementDataStorage::RowHandle DropTargetRowHandle = TEDSItem->GetRowHandle();
		
		for(TypedElementDataStorage::RowHandle RowHandle : DraggedRowHandles)
		{
			// Add the column
			Storage->AddColumn(RowHandle, HierarchyData.GetValue().HierarchyColumn);

			// Let the hierarchy data fill in the parent row into the column
			HierarchyData.GetValue().SetParent.Execute(Storage->GetColumnData(RowHandle, HierarchyData.GetValue().HierarchyColumn), DropTargetRowHandle);
			
			Storage->AddColumn<FTypedElementSyncBackToWorldTag>(RowHandle);
		}
	}
}

TSharedPtr<SWidget> FTypedElementOutlinerMode::CreateContextMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	if (!ToolMenus->IsMenuRegistered(UE::TEDSOutliner::Local::ContextMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu((UE::TEDSOutliner::Local::ContextMenuName));
		Menu->AddDynamicSection("DynamicHierarchySection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			if(UTEDSOutlinerMenuContext* TEDSOutlinerMenuContext = InMenu->FindContext<UTEDSOutlinerMenuContext>())
			{
				if(SSceneOutliner* SceneOutliner = TEDSOutlinerMenuContext->OwningSceneOutliner)
				{
					TArray<FSceneOutlinerTreeItemPtr> Selection = SceneOutliner->GetTree().GetSelectedItems();

					if(Selection.Num() == 1)
					{
						Selection[0]->GenerateContextMenu(InMenu, *SceneOutliner);
					}
				}
			}
			
		}));
	}

	UTEDSOutlinerMenuContext* TEDSOutlinerMenuContext = NewObject<UTEDSOutlinerMenuContext>();
	TEDSOutlinerMenuContext->OwningSceneOutliner = SceneOutliner;
	
	FToolMenuContext MenuContext;
	MenuContext.AddObject(TEDSOutlinerMenuContext);

	return UToolMenus::Get()->GenerateWidget(UE::TEDSOutliner::Local::ContextMenuName, MenuContext);
}

TUniquePtr<ISceneOutlinerHierarchy> FTypedElementOutlinerMode::CreateHierarchy()
{
	return MakeUnique<FTypedElementOutlinerHierarchy>(this, InitialQueryDescription.Get(), HierarchyData);
}

void FTypedElementOutlinerMode::AppendQuery(TypedElementDataStorage::FQueryDescription& Query1, const TypedElementDataStorage::FQueryDescription& Query2)
{
	// TEDS-Outliner TODO: We simply discard duplicate types for now but we probably want a more robust system to detect duplicates and conflicting conditions
	for(int32 i = 0; i < Query2.ConditionOperators.Num(); ++i)
	{
		// Make sure we don't add duplicate conditions
		TypedElementDataStorage::FQueryDescription::FOperator* FoundCondition = Query1.ConditionOperators.FindByPredicate([&Query2, i](const TypedElementDataStorage::FQueryDescription::FOperator& Op)
		{
			return Op.Type == Query2.ConditionOperators[i].Type;
		});

		// We also can't have a duplicate selection type and condition
		TWeakObjectPtr<const UScriptStruct>* FoundSelection = Query1.SelectionTypes.FindByPredicate([&Query2, i](const TWeakObjectPtr<const UScriptStruct>& Selection)
		{
			return Selection == Query2.ConditionOperators[i].Type;
		});

		if(!FoundCondition && !FoundSelection)
		{
			Query1.ConditionOperators.Add(Query2.ConditionOperators[i]);
			Query1.ConditionTypes.Add(Query2.ConditionTypes[i]);
		}
	}
}

bool FTypedElementOutlinerMode::HasItemParentChanged(TypedElementDataStorage::RowHandle InRowHandle, TypedElementDataStorage::RowHandle ParentRowHandle)
{
	const FSceneOutlinerTreeItemPtr Item = SceneOutliner->GetTreeItem(InRowHandle, true);

	// If the item doesn't exist, it doesn't make sense to say its parent changed
	if(!Item)
	{
		return false;
	}
	
	const FSceneOutlinerTreeItemPtr ParentItem = Item->GetParent();

	// If the item doesn't have a parent, but ParentRowHandle is valid: The item just got added a parent so we want to dirty it
	if(!ParentItem)
	{
		return ParentRowHandle != TypedElementDataStorage::InvalidRowHandle;
	}
	
	const FTypedElementOutlinerTreeItem* TEDSParentItem = ParentItem->CastTo<FTypedElementOutlinerTreeItem>();

	if(ensureMsgf(TEDSParentItem, TEXT("The TEDS Outliner should only have FTypedElementOutlinerTreeItems!")))
	{
		// return true if the row handle of the parent item doesn't match what we are given, i.e the parent has changed
		return TEDSParentItem->GetRowHandle() != ParentRowHandle;
	}

	return false;
}

void FTypedElementOutlinerMode::SetRowHandleQuery(TypedElementDataStorage::QueryHandle InRowHandleQuery)
{
	FinalRowHandleQuery = InRowHandleQuery;
	RecompileQueries();
}

void FTypedElementOutlinerMode::AddExternalQuery(FName QueryName, const TypedElementDataStorage::FQueryDescription& InQueryDescription)
{
	ExternalQueries.Emplace(QueryName, InQueryDescription);
}

void FTypedElementOutlinerMode::RemoveExternalQuery(FName QueryName)
{
	ExternalQueries.Remove(QueryName);
}

void FTypedElementOutlinerMode::AppendExternalQueries(TypedElementDataStorage::FQueryDescription& OutQuery)
{
	for(const TPair<FName, TypedElementDataStorage::FQueryDescription>& ExternalQuery : ExternalQueries)
	{
		AppendQuery(OutQuery, ExternalQuery.Value);
	}
}

void FTypedElementOutlinerMode::RecompileQueries()
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;

	// We don't need to register queries to sync selection if there's no selection set attached
	if(!SelectionSetName.IsSet())
	{
		return;					
	}

	UnregisterQueries();
	
	const FQueryDescription& FinalRowHandleQueryDescription = Storage->GetQueryDescription(FinalRowHandleQuery);

	// Query to grab all selected rows
	FQueryDescription SelectedRowsQueryDescription =
					Select()
						.Where()
							.All<FTypedElementSelectionColumn>()
						.Compile();
	
	// Query to track when a row gets selected
	FQueryDescription SelectionAddedQueryDescription =
						Select(
						TEXT("Row selected"),
						FObserver::OnAdd<FTypedElementSelectionColumn>().ForceToGameThread(true),
						[this](IQueryContext& Context, TypedElementRowHandle Row)
						{
							bSelectionDirty = true;
						})
						.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are the rows the user requested
	AppendQuery(SelectionAddedQueryDescription, FinalRowHandleQueryDescription);

	// Query to track when a row gets deselected
	FQueryDescription SelectionRemovedQueryDescription =
						Select(
						TEXT("Row deselected"),
						FObserver::OnRemove<FTypedElementSelectionColumn>().ForceToGameThread(true),
						[this](IQueryContext& Context, TypedElementRowHandle Row)
						{
							bSelectionDirty = true;
						})
						.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are the rows the user requested
	AppendQuery(SelectionRemovedQueryDescription, FinalRowHandleQueryDescription);

	SelectedRowsQuery = Storage->RegisterQuery(MoveTemp(SelectedRowsQueryDescription));
	SelectionAddedQuery = Storage->RegisterQuery(MoveTemp(SelectionAddedQueryDescription));
	SelectionRemovedQuery = Storage->RegisterQuery(MoveTemp(SelectionRemovedQueryDescription));
}

void FTypedElementOutlinerMode::UnregisterQueries()
{
	Storage->UnregisterQuery(SelectedRowsQuery);
	Storage->UnregisterQuery(SelectionAddedQuery);
	Storage->UnregisterQuery(SelectionRemovedQuery);
}

#undef LOCTEXT_NAMESPACE
