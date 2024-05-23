// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsDebuggerModule.h"

#include "LevelEditor.h"
#include "SceneOutlinerPublicTypes.h"
#include "TedsOutlinerModule.h"
#include "TypedElementOutlinerMode.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "TedsOutlinerModule"

FTedsDebuggerModule::FTedsDebuggerModule()
{
	
}


void FTedsDebuggerModule::StartupModule()
{
	IModuleInterface::StartupModule();

	TedsDebuggerTabName = TEXT("TedsDebugger");
	RegisterTabSpawners();
}

void FTedsDebuggerModule::ShutdownModule()
{
	UnregisterTabSpawners();
	
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	if(Registry && Registry->AreDataStorageInterfacesSet())
	{
		Registry->GetMutableDataStorage()->UnregisterQuery(InitialColumnQuery);
	}

	IModuleInterface::ShutdownModule();

}

void FTedsDebuggerModule::RegisterTabSpawners()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		
	LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda([this]()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");

		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

		LevelEditorTabManager->RegisterTabSpawner(TedsDebuggerTabName, FOnSpawnTab::CreateRaw(this, &FTedsDebuggerModule::OpenTedsDebuggerTab))
		.SetDisplayName(LOCTEXT("TedsDebuggerTitle", "TEDS Table View Debugger"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"));
	
	});
}

void FTedsDebuggerModule::UnregisterTabSpawners()
{
}

TSharedRef<SDockTab> FTedsDebuggerModule::OpenTedsDebuggerTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreateTedsDebugger()
		];
}

void FTedsDebuggerModule::NavigateToRow(TypedElementDataStorage::RowHandle InRow)
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	if (!Registry)
	{
		return;
	}
	
	// If the debugger isn't already open, open it
	if(!TedsDebuggerInstance.IsValid())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");

		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		
		LevelEditorTabManager->TryInvokeTab(TedsDebuggerTabName);
	}

	TSharedPtr<ISceneOutliner> TedsDebuggerPinned = TedsDebuggerInstance.Pin();
	if(!TedsDebuggerPinned)
	{
		return;
	}

	// If the item isn't currently present in the debugger, try disabling all filters to make it show up
	if(!TedsDebuggerPinned->GetTreeItem(InRow))
	{
		TedsDebuggerPinned->DisableAllFilterBarFilters(/** bRemove */ false);
	}

	// Defer the actual navigation by one tick to give the outliner a chance to update its items in case any filters were disabled
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([TedsDebuggerPinned, InRow](float DeltaTime)
	{
		// Find the item for this row, select it and scroll to view it
		if(FSceneOutlinerTreeItemPtr TreeItem = TedsDebuggerPinned->GetTreeItem(InRow))
		{
			TedsDebuggerPinned->SetSelection([TreeItem](ISceneOutlinerTreeItem& Item)
			{
				return Item.GetID() == TreeItem->GetID();
			});
			
			TedsDebuggerPinned->FrameSelectedItems();
		}
		
		return false;
	}));
}

TSharedRef<SWidget> FTedsDebuggerModule::CreateTedsDebugger()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("Unable to initialize the table viewer before TEDS is initialized."));

	if(!Registry->AreDataStorageInterfacesSet())
	{
		return SNew(STextBlock)
		.Text(LOCTEXT("TEDSPluginNotEnabledText", "You need to enable the Typed Element Data Storage plugin to see the table viewer!"));
	}

	using namespace TypedElementQueryBuilder;

	// The TEDS-Debugger will show all rows with a label
	TypedElementDataStorage::FQueryDescription RowQueryDescription =
						Select()
						.Where()
							.All<FTypedElementLabelColumn>()
						.Compile();

	// TEDS-Debugger TODO: Currently uses a pre-determined initial set of columns, how can we drive this by the rows shown or let the user pick?
	TypedElementDataStorage::FQueryDescription ColumnQueryDescription =
						Select()
							.ReadOnly<FTypedElementClassTypeInfoColumn, FTypedElementSelectionColumn, FTypedElementRowReferenceColumn>()
						.Compile();

	InitialColumnQuery = Registry->GetMutableDataStorage()->RegisterQuery(MoveTemp(ColumnQueryDescription));

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = true;
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.OutlinerIdentifier = "TedsDebugger";

	FTypedElementOutlinerModeParams Params(nullptr);
	Params.QueryDescription = RowQueryDescription;
	Params.bUseDefaultTedsFilters = true;
	Params.HierarchyData = TOptional<FTypedElementOutlinerHierarchyData>(); // We don't want to show hierarchies in the debugger

	FTedsOutlinerModule& TedsOutlinerModule = FModuleManager::GetModuleChecked<FTedsOutlinerModule>("TedsOutliner");
	
	TSharedRef<ISceneOutliner> TedsOutliner = TedsOutlinerModule.CreateTedsOutliner(InitOptions, Params, InitialColumnQuery);

	// Store an instance of the global Teds Debugger
	TedsDebuggerInstance = TedsOutliner;
	
	return TedsOutliner;
}

IMPLEMENT_MODULE(FTedsDebuggerModule, TedsDebugger);

#undef LOCTEXT_NAMESPACE