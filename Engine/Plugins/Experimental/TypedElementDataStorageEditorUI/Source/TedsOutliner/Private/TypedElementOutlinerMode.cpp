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
{
	using namespace TypedElementQueryBuilder;
	
	TedsOutlinerImpl = MakeShared<FTedsOutlinerImpl>(InParams, this);
	TedsOutlinerImpl->Init();
	TedsOutlinerImpl->OnSelectionChanged().AddRaw(this, &FTypedElementOutlinerMode::OnSelectionChanged);
	
	TedsOutlinerImpl->IsItemCompatible().BindLambda([](const ISceneOutlinerTreeItem& Item)
	{
		return Item.IsA<FTypedElementOutlinerTreeItem>();
	});
}

FTypedElementOutlinerMode::~FTypedElementOutlinerMode()
{
	
}

void FTypedElementOutlinerMode::Rebuild()
{
	Hierarchy = CreateHierarchy();
}

void FTypedElementOutlinerMode::OnSelectionChanged()
{
	TOptional<FName> SelectionSetName = TedsOutlinerImpl->GetSelectionSetName();
	ITypedElementDataStorageInterface* Storage = TedsOutlinerImpl->GetStorage();
	
	// The selection in TEDS was changed, update the outliner to respond
	SceneOutliner->SetSelection([SelectionSetName, Storage](ISceneOutlinerTreeItem& InItem) -> bool
	{
		if(const FTypedElementOutlinerTreeItem* TEDSItem = InItem.CastTo<FTypedElementOutlinerTreeItem>())
		{
			const TypedElementDataStorage::RowHandle RowHandle = TEDSItem->GetRowHandle();

			if(const FTypedElementSelectionColumn* SelectionColumn = Storage->GetColumn<FTypedElementSelectionColumn>(RowHandle))
			{
				return SelectionColumn->SelectionSet == SelectionSetName;
			}
		}
		return false;
	});
}

void FTypedElementOutlinerMode::SynchronizeSelection()
{
	OnSelectionChanged();
}

void FTypedElementOutlinerMode::OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection)
{
	if(SelectionType == ESelectInfo::Direct)
	{
		return; // Direct selection means we selected from outside the Outliner i.e through TEDS, so we don't need to redo the column addition
	}

	TArray<TypedElementDataStorage::RowHandle> RowHandles;
	
	// The selection in the Outliner changed, update TEDS
	Selection.ForEachItem([&RowHandles](FSceneOutlinerTreeItemPtr& Item)
	{
		if(FTypedElementOutlinerTreeItem* TEDSItem = Item->CastTo<FTypedElementOutlinerTreeItem>())
		{
			RowHandles.Add(TEDSItem->GetRowHandle());
		}
	});

	TedsOutlinerImpl->SetSelection(RowHandles);
}

TSharedPtr<FDragDropOperation> FTypedElementOutlinerMode::CreateDragDropOperation(const FPointerEvent& MouseEvent, const TArray<FSceneOutlinerTreeItemPtr>& InTreeItems) const
{
	const TOptional<FTypedElementOutlinerHierarchyData>& HierarchyData = TedsOutlinerImpl->GetHierarchyData();

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
	const TOptional<FTypedElementOutlinerHierarchyData>& HierarchyData = TedsOutlinerImpl->GetHierarchyData();
	ITypedElementDataStorageInterface* Storage = TedsOutlinerImpl->GetStorage();

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
	const TOptional<FTypedElementOutlinerHierarchyData>& HierarchyData = TedsOutlinerImpl->GetHierarchyData();
	ITypedElementDataStorageInterface* Storage = TedsOutlinerImpl->GetStorage();

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
	return MakeUnique<FTypedElementOutlinerHierarchy>(this, TedsOutlinerImpl.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
