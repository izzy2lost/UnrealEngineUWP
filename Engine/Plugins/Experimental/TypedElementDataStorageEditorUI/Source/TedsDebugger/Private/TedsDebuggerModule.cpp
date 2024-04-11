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
							.ReadOnly<FTypedElementClassTypeInfoColumn, FTypedElementSlateWidgetReferenceColumn, FTypedElementRowReferenceColumn>()
						.Compile();

	InitialColumnQuery = Registry->GetMutableDataStorage()->RegisterQuery(MoveTemp(ColumnQueryDescription));

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = true;
	InitOptions.bShowSearchBox = false; // TEDS-Debugger TODO: Enable after search support
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.OutlinerIdentifier = "TedsDebugger";

	FTypedElementOutlinerModeParams Params(nullptr);
	Params.QueryDescription = RowQueryDescription;
	Params.bUseDefaultTEDSFilters = true;

	// TEDS-Debugger TODO: We'll keep this synced with the level editor for now, because TEDS currently only supports one selection set per row
	// and if we used a unique one it would deselect it from the level editor's set in TEDS only causing it to go out of sync with the TEv1 selection
	Params.SelectionSetOverride = FName();
	
	FTedsOutlinerModule& TedsOutlinerModule = FModuleManager::GetModuleChecked<FTedsOutlinerModule>("TedsOutliner");
	
	return TedsOutlinerModule.CreateTedsOutliner(InitOptions, Params, InitialColumnQuery);
}

IMPLEMENT_MODULE(FTedsDebuggerModule, TedsDebugger);

#undef LOCTEXT_NAMESPACE