// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolkits/DMXControlConsoleEditorToolkit.h"

#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorModule.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleEditorToolbar.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXEditorSettings.h"
#include "Framework/Commands/GenericCommands.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorDetailsView.h"
#include "Views/SDMXControlConsoleEditorDMXLibraryView.h"
#include "Views/SDMXControlConsoleEditorLayoutView.h"
#include "Widgets/Docking/SDockTab.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorToolkit"

namespace UE::DMX::ControlConsoleEditor::Private
{
	const FName FDMXControlConsoleEditorToolkit::DMXLibraryViewTabID(TEXT("DMXControlConsoleEditorToolkit_DMXLibraryViewTabID"));
	const FName FDMXControlConsoleEditorToolkit::LayoutViewTabID(TEXT("DMXControlConsoleEditorToolkit_LayoutViewTabID"));
	const FName FDMXControlConsoleEditorToolkit::DetailsViewTabID(TEXT("DMXControlConsoleEditorToolkit_DetailsViewTabID"));

	FDMXControlConsoleEditorToolkit::FDMXControlConsoleEditorToolkit()
		: ControlConsole(nullptr)
	{
	}

	void FDMXControlConsoleEditorToolkit::InitControlConsoleEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDMXControlConsole* InControlConsole)
	{
		checkf(InControlConsole, TEXT("Invalid control console, can't initialize toolkit correctly."));
		ControlConsole = InControlConsole;

		EditorModel = NewObject<UDMXControlConsoleEditorModel>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
		EditorModel->Initialize(SharedThis(this));

		UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
		if (DMXEditorSettings)
		{
			DMXEditorSettings->LastOpenedControlConsolePath = ControlConsole->GetPathName();
			DMXEditorSettings->SaveConfig();
		}

		InitializeInternal(Mode, InitToolkitHost, FGuid::NewGuid());
	}

	UDMXControlConsoleData* FDMXControlConsoleEditorToolkit::GetControlConsoleData() const
	{
		return ControlConsole ? ControlConsole->GetControlConsoleData() : nullptr;
	}

	UDMXControlConsoleEditorData* FDMXControlConsoleEditorToolkit::GetControlConsoleEditorData() const
	{
		return ControlConsole ? Cast<UDMXControlConsoleEditorData>(ControlConsole->ControlConsoleEditorData) : nullptr;
	}

	UDMXControlConsoleEditorLayouts* FDMXControlConsoleEditorToolkit::GetControlConsoleLayouts() const
	{
		return ControlConsole ? Cast<UDMXControlConsoleEditorLayouts>(ControlConsole->ControlConsoleEditorLayouts) : nullptr;
	}

	void FDMXControlConsoleEditorToolkit::ToggleSendDMX()
	{
		UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
		if (!ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, can't send dmx correctly.")))
		{
			return;
		}

		if (ControlConsoleData->IsSendingDMX())
		{
			ControlConsoleData->StopSendingDMX();
		}
		else
		{
			ControlConsoleData->StartSendingDMX();
		}
	}

	bool FDMXControlConsoleEditorToolkit::IsSendingDMX() const
	{
		UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
		if (ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, cannot deduce if it is sending DMX.")))
		{
			return ControlConsoleData->IsSendingDMX();
		}
		return false;
	}

	void FDMXControlConsoleEditorToolkit::RemoveAllSelectedElements()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
		if (!ControlConsoleLayouts || !EditorModel)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
		if (SelectedFaderGroupsObjects.IsEmpty())
		{
			return;
		}

		const FScopedTransaction RemoveAllSelectedElementsTransaction(LOCTEXT("RemoveAllSelectedElementsTransaction", "Selected Elements removed"));

		// Delete all selected fader groups
		for (const TWeakObjectPtr<UObject>& SelectedFaderGroupObject : SelectedFaderGroupsObjects)
		{
			UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject);
			if (SelectedFaderGroup &&
				SelectionHandler->GetSelectedFadersFromFaderGroup(SelectedFaderGroup).IsEmpty())
			{
				// If there's only one fader group to delete, replace it in selection
				if (SelectedFaderGroupsObjects.Num() == 1)
				{
					SelectionHandler->ReplaceInSelection(SelectedFaderGroup);
				}

				constexpr bool bNotifySelectedFaderGroupChange = false;
				SelectionHandler->RemoveFromSelection(SelectedFaderGroup, bNotifySelectedFaderGroupChange);

				ActiveLayout->PreEditChange(nullptr);
				ActiveLayout->RemoveFromLayout(SelectedFaderGroup);
				ActiveLayout->RemoveFromActiveFaderGroups(SelectedFaderGroup);
				ActiveLayout->PostEditChange();

				if (!SelectedFaderGroup->HasFixturePatch())
				{
					SelectedFaderGroup->Destroy();
				}
			}
		}

		// Delete all selected faders
		const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
		if (!SelectedFadersObjects.IsEmpty())
		{
			for (TWeakObjectPtr<UObject> SelectedFaderObject : SelectedFadersObjects)
			{
				UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectedFaderObject);
				const bool bValidFaderWithPatch = SelectedFader && !SelectedFader->GetOwnerFaderGroupChecked().HasFixturePatch();
				if (!bValidFaderWithPatch)
				{
					continue;
				}

				// If there's only one fader to delete, replace it in selection
				if (SelectedFadersObjects.Num() == 1)
				{
					SelectionHandler->ReplaceInSelection(SelectedFader);
				}

				constexpr bool bNotifyFaderSelectionChange = false;
				SelectionHandler->RemoveFromSelection(SelectedFader, bNotifyFaderSelectionChange);

				SelectedFader->Destroy();
			}
		}

		SelectionHandler->RemoveInvalidObjectsFromSelection();
	}

	void FDMXControlConsoleEditorToolkit::ClearAll()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid control console layouts, cannot clear the active layout correctly.")))
		{
			return;
		}

		if (!ensureMsgf(EditorModel, TEXT("Invalid control console editor model, cannot clear the active layout correctly.")))
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const FScopedTransaction ClearAllTransaction(LOCTEXT("ClearAllTransaction", "Clear All"));
		ActiveLayout->Modify();
		ActiveLayout->ClearAll();
		if (ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
			for (UDMXControlConsoleEditorGlobalLayoutBase* UserLayout : UserLayouts)
			{
				UserLayout->Modify();
				constexpr bool bOnlyPatchedFaderGroups = true;
				UserLayout->ClearAll(bOnlyPatchedFaderGroups);
			}

			if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
			{
				ControlConsoleData->Modify();
				constexpr bool bOnlyPatchedFaderGroups = true;
				ControlConsoleData->ClearAll(bOnlyPatchedFaderGroups);
			}
		}
	}

	void FDMXControlConsoleEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_ControlConsoleEditor", "DMX Control Console Editor"));
		TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

		FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

		InTabManager->RegisterTabSpawner(DMXLibraryViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_DMXLibraryView))
			.SetDisplayName(LOCTEXT("Tab_DMXLibraryView", "DMX Library"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.DMXLibrary"));

		InTabManager->RegisterTabSpawner(LayoutViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_LayoutView))
			.SetDisplayName(LOCTEXT("Tab_LayoutView", "Layout Editor"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.TabIcon"));

		InTabManager->RegisterTabSpawner(DetailsViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_DetailsView))
			.SetDisplayName(LOCTEXT("Tab_EditorView", "Details"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Details"));
	}

	void FDMXControlConsoleEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
	{
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

		InTabManager->UnregisterTabSpawner(DMXLibraryViewTabID);
		InTabManager->UnregisterTabSpawner(LayoutViewTabID);
		InTabManager->UnregisterTabSpawner(DetailsViewTabID);
	}

	const FSlateBrush* FDMXControlConsoleEditorToolkit::GetDefaultTabIcon() const
	{
		return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.TabIcon");
	}

	FText FDMXControlConsoleEditorToolkit::GetBaseToolkitName() const
	{
		return LOCTEXT("AppLabel", "DMX Control Console");
	}

	FName FDMXControlConsoleEditorToolkit::GetToolkitFName() const
	{
		return FName("DMXControlConsole");
	}

	FString FDMXControlConsoleEditorToolkit::GetWorldCentricTabPrefix() const
	{
		return LOCTEXT("WorldCentricTabPrefix", "DMX Control Console ").ToString();
	}

	void FDMXControlConsoleEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(EditorModel);
		Collector.AddReferencedObject(ControlConsole);
	}

	FString FDMXControlConsoleEditorToolkit::GetReferencerName() const
	{
		return TEXT("FDMXControlConsoleEditorToolkit");
	}

	void FDMXControlConsoleEditorToolkit::InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, const FGuid& MessageLogGuid)
	{
		if (!ControlConsole)
		{
			return;
		}

		GenerateInternalViews();

		TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_ControlConsole_Layout_1.2")
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DMXLibraryViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(.2f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(LayoutViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(.6f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DetailsViewTabID, ETabState::SidebarTab, ESidebarLocation::Right, .2f)
						->SetSizeCoefficient(.2f)
					)
				)
			);

		const bool bCreateDefaultStandaloneMenu = true;
		const bool bCreateDefaultToolbar = true;
		FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FDMXControlConsoleEditorModule::ControlConsoleEditorAppIdentifier,
			StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ControlConsole);

		SetupCommands();
		ExtendToolbar();
		RegenerateMenusAndToolbars();
	}

	void FDMXControlConsoleEditorToolkit::GenerateInternalViews()
	{
		GenerateDMXLibraryView();
		GenerateLayoutView();
		GenerateDetailsView();
	}

	TSharedRef<SDMXControlConsoleEditorDMXLibraryView> FDMXControlConsoleEditorToolkit::GenerateDMXLibraryView()
	{
		if (!DMXLibraryView.IsValid())
		{
			DMXLibraryView = SNew(SDMXControlConsoleEditorDMXLibraryView, EditorModel);
		}

		return DMXLibraryView.ToSharedRef();
	}

	TSharedRef<SDMXControlConsoleEditorLayoutView> FDMXControlConsoleEditorToolkit::GenerateLayoutView()
	{
		if (!LayoutView.IsValid())
		{
			LayoutView = SNew(SDMXControlConsoleEditorLayoutView, EditorModel);
		}

		return LayoutView.ToSharedRef();
	}

	TSharedRef<SDMXControlConsoleEditorDetailsView> FDMXControlConsoleEditorToolkit::GenerateDetailsView()
	{
		if (!DetailsView.IsValid())
		{
			DetailsView = SNew(SDMXControlConsoleEditorDetailsView, EditorModel);
		}

		return DetailsView.ToSharedRef();
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_DMXLibraryView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == DMXLibraryViewTabID);

		TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("DMXLibraryViewTabID", "DMX Library"))
			[
				DMXLibraryView.ToSharedRef()
			];

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_LayoutView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == LayoutViewTabID);

		TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("LayoutViewTabID", "Layout Editor"))
			[
				LayoutView.ToSharedRef()
			];

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_DetailsView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == DetailsViewTabID);

		TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("DetailsViewTabID", "Details"))
			[
				DetailsView.ToSharedRef()
			];

		return SpawnedTab;
	}

	void FDMXControlConsoleEditorToolkit::SetupCommands()
	{
		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().ToggleSendDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::ToggleSendDMX),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FDMXControlConsoleEditorToolkit::IsSendingDMX)
		);

		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().RemoveElements,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::RemoveAllSelectedElements)
		);

		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().ClearAll,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::ClearAll)
		);

		if (EditorModel)
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			constexpr bool bSelectOnlyVisible = true;
			GetToolkitCommands()->MapAction
			(
				FDMXControlConsoleEditorCommands::Get().SelectAll,
				FExecuteAction::CreateSP(SelectionHandler, &FDMXControlConsoleEditorSelection::SelectAll, bSelectOnlyVisible)
			);
		}
	}

	void FDMXControlConsoleEditorToolkit::ExtendToolbar()
	{
		Toolbar = MakeShared<FDMXControlConsoleEditorToolbar>(SharedThis(this));

		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		Toolbar->BuildToolbar(ToolbarExtender);
		AddToolbarExtender(ToolbarExtender);
	}
}

#undef LOCTEXT_NAMESPACE
