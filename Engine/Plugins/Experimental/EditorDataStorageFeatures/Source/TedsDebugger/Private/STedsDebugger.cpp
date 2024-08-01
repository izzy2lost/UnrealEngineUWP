// Copyright Epic Games, Inc. All Rights Reserved.

#include "STedsDebugger.h"

#include "SceneOutlinerPublicTypes.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Framework/Docking/LayoutService.h"
#include "Modules/ModuleManager.h"
#include "QueryEditor/TedsQueryEditor.h"
#include "TedsOutlinerModule.h"
#include "TedsOutlinerMode.h"

#define LOCTEXT_NAMESPACE "STedsDebugger"

namespace UE::EditorDataStorage::Debugger::Private
{
	FName QueryEditorToolTabName = TEXT("TEDS Query Editor");
	FName TableViewerToolTabName = TEXT("TEDS Table Viewer");
	FName ToolbarTabName = TEXT("TEDS Debugger Toolbar");
}


STedsDebugger::~STedsDebugger()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	if (Registry && Registry->AreDataStorageInterfacesSet())
	{
		Registry->GetMutableDataStorage()->UnregisterQuery(TableViewerQuery);
	}
}

void STedsDebugger::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	// Create the tab manager for our sub tabsa
	TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);
	TabManager->SetAllowWindowMenuBar(true);

	// Register Tab Spawners
	RegisterTabSpawners();

	using namespace UE::EditorDataStorage::Debugger::Private;

	// Setup the default layout
	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("TedsDebuggerLayout_v0")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->AddTab(ToolbarTabName, ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
					->AddTab(QueryEditorToolTabName, ETabState::OpenedTab)
					->AddTab(TableViewerToolTabName, ETabState::OpenedTab)
			)
		)
	);

	Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);

	ChildSlot
	[
		TabManager->RestoreFrom(Layout, ConstructUnderWindow).ToSharedRef()
	];

	TabManager->SetOnPersistLayout(
		FTabManager::FOnPersistLayout::CreateStatic(
			[](const TSharedRef<FTabManager::FLayout>& InLayout)
			{
				if (InLayout->GetPrimaryArea().Pin().IsValid())
				{
					FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, InLayout);
				}
			}
		)
	);

	// create & initialize main menu
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());

	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &STedsDebugger::FillWindowMenu),
		"Window"
	);

	TSharedRef<SWidget> MenuWidget = MenuBarBuilder.MakeWidget();

	// Tell tab-manager about the multi-box for platforms with a global menu bar
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuWidget);
}

void STedsDebugger::FillWindowMenu( FMenuBuilder& MenuBuilder)
{
	if (TabManager)
	{
		TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
	}
}

TSharedRef<SDockTab> STedsDebugger::SpawnToolbar(const FSpawnTabArgs& Args)
{
	// The toolbar is currently empty but can be used to house tools that are not specific to a specific tab in the debugger
	FSlimHorizontalToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.ShouldAutosize(true)
		[
			ToolBarBuilder.MakeWidget()
		];
}

TSharedRef<SDockTab> STedsDebugger::SpawnQueryEditorTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DockTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	if (!QueryEditorModel)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

		if (Registry && Registry->AreDataStorageInterfacesSet())
		{
			ITypedElementDataStorageInterface* DataStorageInterface = Registry->GetMutableDataStorage();
			QueryEditorModel = MakeUnique<UE::EditorDataStorage::Debug::QueryEditor::FTedsQueryEditorModel>(*DataStorageInterface);
		}
	}
	if (QueryEditorModel)
	{
		QueryEditorModel->Reset();	

		TSharedRef<UE::EditorDataStorage::Debug::QueryEditor::SQueryEditorWidget> QueryEditor =
			SNew(UE::EditorDataStorage::Debug::QueryEditor::SQueryEditorWidget, *QueryEditorModel);
		DockTab->SetContent(QueryEditor);
	}
	else
	{
		TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
		.Text(LOCTEXT("TedsDebuggerModule_CannotLoadQueryEditor", "Cannot load Query Editor - Invalid Model"));
		DockTab->SetContent(TextBlock);
	}


	return DockTab;
}

TSharedRef<SDockTab> STedsDebugger::SpawnTableViewerTab(const FSpawnTabArgs& Args)
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("Unable to initialize the table viewer before TEDS is initialized."));

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

	TableViewerQuery = Registry->GetMutableDataStorage()->RegisterQuery(MoveTemp(ColumnQueryDescription));

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = true;
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.OutlinerIdentifier = "TedsDebugger.TableViewer";

	FTedsOutlinerParams Params(nullptr);
	Params.QueryDescription = RowQueryDescription;
	Params.bUseDefaultTedsFilters = true;
	Params.HierarchyData = TOptional<FTedsOutlinerHierarchyData>(); // We don't want to show hierarchies in the debugger
	Params.CellWidgetPurposes = TArray<FName>{TEXT("General.Cell")};
	
	FTedsOutlinerModule& TedsOutlinerModule = FModuleManager::GetModuleChecked<FTedsOutlinerModule>("TedsOutliner");
	
	TSharedRef<ISceneOutliner> TedsOutliner = TedsOutlinerModule.CreateTedsOutliner(InitOptions, Params, TableViewerQuery);

	// Store an instance of the table viewer
	TableViewerInstance = TedsOutliner;
	
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			TedsOutliner
		];
}

void STedsDebugger::RegisterTabSpawners()
{
	using namespace UE::EditorDataStorage::Debugger::Private;

	const TSharedRef<FWorkspaceItem> AppMenuGroup =
		TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("TedsDebuggerGroupName", "Teds Debugger"));

	TabManager->RegisterTabSpawner(
		ToolbarTabName,
		FOnSpawnTab::CreateRaw(this, &STedsDebugger::SpawnToolbar))
		.SetGroup(AppMenuGroup)
		.SetDisplayName(LOCTEXT("TedsDebugger_ToolbarDisplayName", "Toolbar"))
		.SetAutoGenerateMenuEntry(false);
	
	TabManager->RegisterTabSpawner(
		QueryEditorToolTabName,
		FOnSpawnTab::CreateRaw(this, &STedsDebugger::SpawnQueryEditorTab))
		.SetGroup(AppMenuGroup)
		.SetDisplayName(LOCTEXT("TedsDebugger_QueryEditorDisplayName", "Query Editor"))
		.SetTooltipText(LOCTEXT("TedsDebugger_QueryEditorToolTip", "Opens the TEDS Query Editor"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"));
	
	TabManager->RegisterTabSpawner(
		TableViewerToolTabName,
		FOnSpawnTab::CreateRaw(this, &STedsDebugger::SpawnTableViewerTab))
		.SetGroup(AppMenuGroup)
		.SetDisplayName(LOCTEXT("TedsDebugger_TableViewerDisplayName", "Table Viewer"))
		.SetTooltipText(LOCTEXT("TedsDebugger_TableViewerToolTip", "Opens the TEDS Table Viewer"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
}

void STedsDebugger::NavigateToRow(TypedElementDataStorage::RowHandle InRow) const
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	if (!Registry)
	{
		return;
	}
	
	// If the debugger isn't already open, open it
	if (!TableViewerInstance.IsValid())
	{
		TabManager->TryInvokeTab(UE::EditorDataStorage::Debugger::Private::TableViewerToolTabName);
	}

	TSharedPtr<ISceneOutliner> TableViewerPinned = TableViewerInstance.Pin();
	if (!TableViewerPinned)
	{
		return;
	}

	// If the item isn't currently present in the debugger, try disabling all filters to make it show up
	if (!TableViewerPinned->GetTreeItem(InRow))
	{
		TableViewerPinned->DisableAllFilterBarFilters(/** bRemove */ false);
	}

	// Defer the actual navigation by one tick to give the outliner a chance to update its items in case any filters were disabled
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([TableViewerPinned, InRow](float DeltaTime)
	{
		// Find the item for this row, select it and scroll to view it
		if (FSceneOutlinerTreeItemPtr TreeItem = TableViewerPinned->GetTreeItem(InRow))
		{
			TableViewerPinned->SetSelection([TreeItem](ISceneOutlinerTreeItem& Item)
			{
				return Item.GetID() == TreeItem->GetID();
			});
			
			TableViewerPinned->FrameSelectedItems();
		}
		
		return false;
	}));
}


#undef LOCTEXT_NAMESPACE
