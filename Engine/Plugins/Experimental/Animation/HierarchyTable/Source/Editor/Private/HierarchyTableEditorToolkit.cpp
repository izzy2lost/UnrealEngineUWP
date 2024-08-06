// Copyright Epic Games, Inc. All Rights Reserved.

#include "HierarchyTableEditorToolkit.h"
#include "Widgets/Docking/SDockTab.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "TedsOutlinerModule.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "SceneOutlinerPublicTypes.h"
#include "TedsOutlinerMode.h"
#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "HierarchyTable/Columns/OverrideColumn.h"
#include "HierarchyTableType.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "HierarchyTableEditorModule.h"

#define LOCTEXT_NAMESPACE "HierarchyTableEditorToolkit"

void FHierarchyTableEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	HierarchyTable = Cast<UHierarchyTable>(InObjects[0]);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("HierarchyTableEditorLayout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab("HierarchyTableEditorTableTab", ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab("HierarchyTableEditorDetailsTab", ETabState::OpenedTab)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, "HierarchyTableEditor", Layout, true, true, InObjects);

	ExtendToolbar();
}

void FHierarchyTableEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("HierarchyTableEditor", "Hierarchy Table Editor"));

	InTabManager->RegisterTabSpawner("HierarchyTableEditorTableTab", FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				[
					CreateTedsOutliner()
				];
		}))
		.SetDisplayName(LOCTEXT("HierarchyTable", "Hierarchy Table"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());


	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ HierarchyTable });
	InTabManager->RegisterTabSpawner("HierarchyTableEditorDetailsTab", FOnSpawnTab::CreateLambda([=](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				[
					DetailsView
				];
		}))
		.SetDisplayName(INVTEXT("Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FHierarchyTableEditorToolkit::OnClose()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	ITypedElementDataStorageInterface* DSI = Registry->GetMutableDataStorage();

	for (const TTuple<int32, TypedElementDataStorage::RowHandle>& Row : EntryIndexToHandleMap)
	{
		DSI->RemoveRow(Row.Value);
	}
}

void FHierarchyTableEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner("HierarchyTableEditorTableTab");
	InTabManager->UnregisterTabSpawner("HierarchyTableEditorDetailsTab");
}

TSharedRef<SWidget> FHierarchyTableEditorToolkit::CreateTedsOutliner()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("Unable to initialize the table viewer before TEDS is initialized."));

	if (!Registry->AreDataStorageInterfacesSet())
	{
		return SNew(STextBlock)
			.Text(INVTEXT("You need to enable the Typed Element Data Storage plugin to see the table viewer!"));
	}

	using namespace TypedElementQueryBuilder;

	if (!ensure(HierarchyTable->TableType))
	{
		return SNullWidget::NullWidget;
	}

	FHierarchyTableEditorModule& HierarchyTableModule = FModuleManager::GetModuleChecked<FHierarchyTableEditorModule>("HierarchyTableEditor");
	const UHierarchyTableTypeHandler_Base* Handler = HierarchyTableModule.FindHandler(HierarchyTable->TableType);
	if (!ensureMsgf(Handler, TEXT("Could not find handler for %s, have you forgotten to register it?"), *HierarchyTable->TableType->GetName()))
	{
		return SNullWidget::NullWidget;
	}

	TArray<UScriptStruct*> HierarchyTableTypeColumns = Handler->GetColumns();
	HierarchyTableTypeColumns.Add(FTypedElementOverrideColumn::StaticStruct());

	TypedElementDataStorage::FQueryDescription ColumnQueryDescription =
		Select()
		.ReadOnly(HierarchyTableTypeColumns)
		.Compile();

	InitialColumnQuery = Registry->GetMutableDataStorage()->RegisterQuery(MoveTemp(ColumnQueryDescription));

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = true;
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.OutlinerIdentifier = "HierarchyTableTedsOutliner";

	FTedsOutlinerParams Params(nullptr);
	{
		TypedElementDataStorage::FQueryDescription RowQueryDescription =
			Select()
			.Where()
			.All<FTypedElementOverrideColumn>()
			.Compile();

		Params.QueryDescription = RowQueryDescription;
		Params.CellWidgetPurposes = TArray<FName>{ TEXT("General.Cell") };
		Params.HierarchyData = FTedsOutlinerHierarchyData::GetDefaultHierarchyData();
	}

	FTedsOutlinerModule& TedsOutlinerModule = FModuleManager::GetModuleChecked<FTedsOutlinerModule>("TedsOutliner");

	ITypedElementDataStorageInterface* DSI = Registry->GetMutableDataStorage();
	static TypedElementDataStorage::TableHandle Table = DSI->FindTable(FName("Editor_HierarchyTableTable"));
	
	TArray<UScriptStruct*> BaseHierarchyTableTypeColumns = Handler->GetColumns();

	for (int32 EntryIndex = 0; EntryIndex < HierarchyTable->TableData.Num(); ++EntryIndex)
	{
		FHierarchyTableEntryData* Entry = &HierarchyTable->TableData[EntryIndex];

		TypedElementRowHandle Row = DSI->AddRow(Table);

		FTypedElementOverrideColumn OverrideEntry;
		OverrideEntry.OwnerEntry = Entry;
		OverrideEntry.OwnerTable = HierarchyTable;
		DSI->AddColumn(Row, MoveTemp(OverrideEntry));

		DSI->AddColumn<FTypedElementLabelColumn>(Row, { .Label = Entry->Identifier.ToString() });

		TypedElementRowHandle* ParentRow = EntryIndexToHandleMap.Find(Entry->Parent);
		if (ParentRow)
		{
			DSI->AddColumn<FTableRowParentColumn>(Row, { .Parent = *ParentRow });
		}

		for (const UScriptStruct* Column : BaseHierarchyTableTypeColumns)
		{
			DSI->AddColumn(Row, Column);
		}

		EntryIndexToHandleMap.Add(EntryIndex, Row);
	}

	TSharedRef<ISceneOutliner> TedsOutliner = TedsOutlinerModule.CreateTedsOutliner(InitOptions, Params, InitialColumnQuery);
	return TedsOutliner;
}

void FHierarchyTableEditorToolkit::ExtendToolbar()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	FName ParentName;
	static const FName MenuName = GetToolMenuToolbarName(ParentName);

	UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu(MenuName);
	const FToolMenuInsert SectionInsertLocation("Asset", EToolMenuInsertType::After);
	{
		FToolMenuSection& HierarchyTableSection = ToolMenu->AddSection("HierarchyTable", LOCTEXT("HierarchyTable_ToolbarLabel", "HierarchyTable"), SectionInsertLocation);

		/*
		HierarchyTableSection.AddEntry(FToolMenuEntry::InitComboButton(
			"AddCurve",
			FUIAction(),
			FNewToolMenuWidget::CreateLambda([this](const FToolMenuContext& InContext) -> TSharedRef<SWidget>
				{
					return SNew(SAnimCurvePicker, HierarchyTable->Skeleton)
						.OnCurvePicked_Lambda([this](const FName SelectedCurve)
							{
								AddCurveEntry(SelectedCurve);
								FSlateApplication::Get().DismissAllMenus();
							});
				}),
			LOCTEXT("AddCurve_Label", "Add Curve"),
			LOCTEXT("AddCurve_ToolTip", "Add a new curve to the hierarchy"),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Plus")
		));
		*/
	}
}

void FHierarchyTableEditorToolkit::AddCurveEntry(const FName CurveName)
{
	// TODO: Reimplement

	/*
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	ITypedElementDataStorageInterface* DSI = Registry->GetMutableDataStorage();
	static TypedElementDataStorage::TableHandle Table = DSI->FindTable(FName("Editor_HierarchyTableTable"));

	TypedElementDataStorage::RowHandle* RootBoneHandle = EntryIndexToHandleMap.Find(0);
	if (!ensure(RootBoneHandle))
	{
		return;
	}

	TypedElementRowHandle Row = DSI->AddRow(Table);
	DSI->AddColumn<FTypedElementOverrideColumn>(Row, { .bIsOverridden = false });
	DSI->AddColumn<FTypedElementLabelColumn>(Row, { .Label = CurveName.ToString() });
	DSI->AddColumn<FTypedElementMetadataColumn>(Row,
		{
			.OwnerTable = HierarchyTable,
			.Children = TArray<TypedElementDataStorage::RowHandle>(),
			.Type = EHierarchyTableMetadataType::Curve,
			.BoneIndex = INDEX_NONE,
			.CurveName = CurveName
		});
	DSI->AddColumn<FTypedElementParentColumn>(Row, { .Parent = *RootBoneHandle });
	
	FTypedElementMetadataColumn* MetadataColumn = DSI->GetColumn<FTypedElementMetadataColumn>(*RootBoneHandle);
	ensure(MetadataColumn);
	MetadataColumn->Children.Add(Row);

	const UHierarchyTableTypeHandler_Base* Handler = GetDefault<UHierarchyTableTypeRegistry>()->FindHandler(HierarchyTable->TableType);
	check(Handler);

	Handler->AddData(DSI, Row, INDEX_NONE, CurveName, HierarchyTable);
	*/
}

#undef LOCTEXT_NAMESPACE