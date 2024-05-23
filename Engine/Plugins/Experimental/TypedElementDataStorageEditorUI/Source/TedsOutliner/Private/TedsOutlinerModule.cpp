// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsOutlinerModule.h"

#include "LevelEditor.h"
#include "SceneOutlinerPublicTypes.h"
#include "TypedElementOutlinerColumnIntegration.h"
#include "TypedElementOutlinerMode.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Elements/Columns/TypedElementAlertColumns.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Modules/ModuleManager.h"
#include "Widgets/TedsOutlinerRowHandleColumn.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "TedsOutlinerModule"

namespace UE::TedsOutliner
{
	static bool bUseNewSCCWidgets = false;

	void RefreshLevelEditorTedsOutliner(bool bAlwaysInvoke)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		FTedsOutlinerModule& TedsOutlinerModule = FModuleManager::GetModuleChecked<FTedsOutlinerModule>("TedsOutliner");
		FName TabId = TedsOutlinerModule.GetTedsOutlinerTabName();

		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		if(bAlwaysInvoke || LevelEditorTabManager->FindExistingLiveTab(TabId))
		{
			LevelEditorTabManager->TryInvokeTab(TabId);
		}
	}
	
	static FAutoConsoleVariableRef CVarUseNewSCCWidgets(
		TEXT("TEDS.UI.UseNewSCCWidgets"),
		UE::TedsOutliner::bUseNewSCCWidgets,
		TEXT("Use new TEDS-based source control widgets in the Outliner (requires TEDS-Outliner to be enabled)")
		, FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			RefreshLevelEditorTedsOutliner(false);
		}));

	// CVar to summon the TEDS-Outliner as a separate tab
	static FAutoConsoleCommand OpenTableViewerConsoleCommand(
		TEXT("TEDS.UI.OpenTableViewer"),
		TEXT("Spawn the test TEDS-Outliner Integration."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			RefreshLevelEditorTedsOutliner(true);
		}));
}

FTedsOutlinerModule::FTedsOutlinerModule()
{
}

TSharedRef<ISceneOutliner> FTedsOutlinerModule::CreateTedsOutliner(const FSceneOutlinerInitializationOptions& InInitOptions, const FTypedElementOutlinerModeParams& InInitTedsOptions, TypedElementDataStorage::QueryHandle ColumnQuery) const
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	ensureMsgf(Registry&& Registry->AreDataStorageInterfacesSet(), TEXT("Unable to initialize the Teds-Outliner before TEDS itself is initialized."));

	FSceneOutlinerInitializationOptions InitOptions(InInitOptions);
	FTypedElementOutlinerModeParams InitTedsOptions(InInitTedsOptions);

	InitOptions.ModeFactory = FCreateSceneOutlinerMode::CreateLambda([&InitTedsOptions](SSceneOutliner* Outliner)
	{
		using namespace TypedElementQueryBuilder;
		InitTedsOptions.SceneOutliner = Outliner;
		
		return new FTypedElementOutlinerMode(InitTedsOptions);
	});

	// Add the custom column that displays row handles
	InitOptions.ColumnMap.Add(FTedsOutlinerRowHandleColumn::GetID(),
		FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 2,
			FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& InSceneOutliner)
			{
				return MakeShareable(new FTedsOutlinerRowHandleColumn(InSceneOutliner));
			})));
	
	InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10));

	
	TSharedRef<ISceneOutliner> TedsOutlinerShared = SNew(SSceneOutliner, InitOptions);
	
	FTypedElementSceneOutlinerQueryBinder::GetInstance().AssignQuery(ColumnQuery, TedsOutlinerShared);
	
	return TedsOutlinerShared;
}

void FTedsOutlinerModule::StartupModule()
{
	IModuleInterface::StartupModule();

	TedsOutlinerTabName = TEXT("LevelEditorTedsOutliner");
	RegisterLevelEditorTedsOutlinerTab();
}

void FTedsOutlinerModule::ShutdownModule()
{
	UnregisterLevelEditorTedsOutlinerTab();
	IModuleInterface::ShutdownModule();
}

TypedElementDataStorage::QueryHandle FTedsOutlinerModule::GetLevelEditorTedsOutlinerColumnQuery()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	ITypedElementDataStorageInterface* Storage = Registry->GetMutableDataStorage();
		
	using namespace TypedElementQueryBuilder;

	static TypedElementDataStorage::QueryHandle ColumnQuery = Storage->RegisterQuery(
		Select()
			.ReadOnly<FTypedElementClassTypeInfoColumn, FTypedElementAlertColumn, FTypedElementChildAlertColumn, FTypedElementRowReferenceColumn>()
		.Compile());

	// Query to also include SCC info
	static TypedElementDataStorage::QueryHandle SCCQuery = Storage->RegisterQuery(
		Select()
			.ReadOnly<FTypedElementClassTypeInfoColumn, FTypedElementPackageReference, FTypedElementAlertColumn, FTypedElementChildAlertColumn>()
		.Compile());

	return UE::TedsOutliner::bUseNewSCCWidgets ? SCCQuery : ColumnQuery;
	
}

TSharedRef<SWidget> FTedsOutlinerModule::CreateLevelEditorTedsOutliner()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("Unable to initialize the table viewer before TEDS is initialized."));

	if(!Registry->AreDataStorageInterfacesSet())
	{
		return SNew(STextBlock)
		.Text(LOCTEXT("TEDSPluginNotEnabledText", "You need to enable the Typed Element Data Storage plugin to see the table viewer!"));
	}

	using namespace TypedElementQueryBuilder;

	// The Outliner is populated with Actors and Entities
	TypedElementDataStorage::FQueryDescription OutlinerQueryDescription =
						Select()
						.Where()
							.All<FTypedElementClassTypeInfoColumn>() // TEDS-Outliner TODO: Currently looking at all entries with type info in TEDS
						.Compile();

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = true;
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.bShowTransient = true;
	InitOptions.OutlinerIdentifier = "TEDSOutliner";

	FTypedElementOutlinerModeParams Params(nullptr);
	Params.QueryDescription = OutlinerQueryDescription;
	Params.bUseDefaultTedsFilters = true;

	// Example Query to filter for actors
	TypedElementDataStorage::FQueryDescription ActorFilterQuery =
					Select()
					.Where()
						.All<FTypedElementActorTag>()
					.Compile();
	Params.FilterQueries.Emplace("Actors", ActorFilterQuery);
		
	// Empty selection set name is currently the level editor
	Params.SelectionSetOverride = FName();
	
	TSharedRef<ISceneOutliner> TEDSOutlinerShared = CreateTedsOutliner(InitOptions, Params, GetLevelEditorTedsOutlinerColumnQuery());
	
	return TEDSOutlinerShared;
}

TSharedRef<SDockTab> FTedsOutlinerModule::OpenLevelEditorTedsOutliner(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreateLevelEditorTedsOutliner()
		];
}

// The TEDS-Outliner as a separate tab
void FTedsOutlinerModule::RegisterLevelEditorTedsOutlinerTab()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		
	LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda([this]()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");

		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

		LevelEditorTabManager->RegisterTabSpawner(TedsOutlinerTabName, FOnSpawnTab::CreateRaw(this, &FTedsOutlinerModule::OpenLevelEditorTedsOutliner))
		.SetDisplayName(LOCTEXT("TedsTableVIewerTitle", "Table Viewer (Experimental)"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorOutlinerCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"))
		.SetAutoGenerateMenuEntry(false); // This can only be summoned from the Cvar now
	
	});
}

void FTedsOutlinerModule::UnregisterLevelEditorTedsOutlinerTab()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
}

FName FTedsOutlinerModule::GetTedsOutlinerTabName()
{
	return TedsOutlinerTabName;
}

IMPLEMENT_MODULE(FTedsOutlinerModule, TedsOutliner);

#undef LOCTEXT_NAMESPACE // TedsOutlinerModule
