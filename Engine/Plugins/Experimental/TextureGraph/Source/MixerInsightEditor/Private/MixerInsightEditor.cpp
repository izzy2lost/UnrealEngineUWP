// Copyright Epic Games, Inc. All Rights Reserved.

#include "MixerInsightEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "LevelEditor.h"
#include "MixerEngine.h"
#include "MixerInsight.h"
#include "MixerInsightEditorCommands.h"
#include "MixerInsightEditorStyle.h"
#include "ToolMenus.h"
#include "View/SMixerInsightActionView.h"
#include "View/SMixerInsightDeviceView.h"
#include "View/SMixerInsightInspectorView.h"
#include "View/SMixerInsightMixView.h"
#include "View/SMixerInsightResourceView.h"
#include "View/SMixerInsightSessionView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


static const FName MixerInsightEditorTabName("MixerInsightEditor");
static const FName MixerInsightEditorTabName_Mixes("MixerInsightEditor_Mixes");
static const FName MixerInsightEditorTabName_Actions("MixerInsightEditor_Actions");
static const FName MixerInsightEditorTabName_JobBatches("MixerInsightEditor_JobBatches");
static const FName MixerInsightEditorTabName_Resources("MixerInsightEditor_Resources");
static const FName MixerInsightEditorTabName_Devices("MixerInsightEditor_Devices");
static const FName MixerInsightEditorTabName_Inspector("MixerInsightEditor_Inspector");

#define LOCTEXT_NAMESPACE "FMixerInsightEditorModule"


struct FInsightTabCommands : public TCommands<FInsightTabCommands>
{
	FInsightTabCommands()
		: TCommands<FInsightTabCommands>(
			TEXT("MixerInsightTab"), // Context name for fast lookup
			LOCTEXT("MixerInsightTab", "MixerInsightTab Debugger"), // Localized context name for displaying
			NAME_None, // Parent
			FCoreStyle::Get().GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

	TSharedPtr<FUICommandInfo> ShowMixesTab;
	TSharedPtr<FUICommandInfo> ShowActionsTab;
	TSharedPtr<FUICommandInfo> ShowJobBatchesTab;
	TSharedPtr<FUICommandInfo> ShowResourcesTab;
	TSharedPtr<FUICommandInfo> ShowDevicesTab;
	TSharedPtr<FUICommandInfo> ShowInspectorTab;
};

void FInsightTabCommands::RegisterCommands()
{
	UI_COMMAND(ShowMixesTab, "Mixes", "Toggles visibility of the Mixes tab", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowActionsTab, "Actions", "Toggles visibility of the Actions tab", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowJobBatchesTab, "JobBatches", "Toggles visibility of the JobBatches tab", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowResourcesTab, "Resources", "Toggles visibility of the Resources tab", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowDevicesTab, "Devices", "Toggles visibility of the Devices tab", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowInspectorTab, "Inspector", "Toggles visibility of the Inspector tab", EUserInterfaceActionType::Check, FInputChord());
}

void FMixerInsightEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FMixerInsightEditorStyle::Initialize();
	FMixerInsightEditorStyle::ReloadTextures();

	FMixerInsightEditorCommands::Register();
	
	Commands = MakeShareable(new FUICommandList);

	// Commands->MapAction(
	// 	FMixerInsightEditorCommands::Get().OpenPluginWindow,
	// 	FExecuteAction::CreateRaw(this, &FMixerInsightEditorModule::PluginButtonClicked),
	// 	FCanExecuteAction());

	// UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMixerInsightEditorModule::RegisterMenus));

	FInsightTabCommands::Register();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MixerInsightEditorTabName, FOnSpawnTab::CreateRaw(this, &FMixerInsightEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FMixerInsightEditorTabTitle", "Mixer Insight"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);


	// in StartupModule()
	_tickDelegate = FTickerDelegate::CreateRaw(this, &FMixerInsightEditorModule::Tick);
	_tickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(_tickDelegate);
}

void FMixerInsightEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FTSTicker::GetCoreTicker().RemoveTicker(_tickDelegateHandle);

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	MixerInsight::Destroy();

	FMixerInsightEditorStyle::Shutdown();

	FMixerInsightEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MixerInsightEditorTabName);
}
bool FMixerInsightEditorModule::Tick(float DeltaTime)
{
	return true;
}
TSharedRef<SDockTab> FMixerInsightEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// (re)Instantiate the Mixer Insight singleton
	if (MixerInsight::Instance())
	{
		MixerInsight::Destroy();
	}
	MixerInsight::Create();


	const TSharedRef<SDockTab> NomadTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(NSLOCTEXT("MixerInsight", "Mixer Insight", "Mixer Insight"));

	if (!TabManager.IsValid())
	{
		TabManager = FGlobalTabmanager::Get()->NewTabManager(NomadTab);
		// on persist layout will handle saving layout if the editor is shut down:
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
	}
	else
	{
		ensure(Layout.IsValid());
	}

	TWeakPtr<FTabManager> tabManagerWeak = TabManager;
	// On tab close will save the layout if the debugging window itself is closed,
	// this handler also cleans up any floating debugging controls. If we don't close
	// all areas we need to add some logic to the tab manager to reuse existing tabs:
	NomadTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateStatic(
		[](TSharedRef<SDockTab> Self, TWeakPtr<FTabManager> InTabManager)
		{
			TSharedPtr<FTabManager> OwningTabManager = InTabManager.Pin();
			if (OwningTabManager.IsValid())
			{
				FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, OwningTabManager->PersistLayout());
				OwningTabManager->CloseAllAreas();
			}
		}
		, tabManagerWeak
	));

	if (!Layout.IsValid())
	{
		TabManager->RegisterTabSpawner(MixerInsightEditorTabName_Mixes, FOnSpawnTab::CreateStatic(
			[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(LOCTEXT("FMixerInsightEditorTabTitleMixes", "Mixer Mixes"))
					[
						SNew(SMixerInsightMixListView)
					];
			}));
		TabManager->RegisterTabSpawner(MixerInsightEditorTabName_Actions, FOnSpawnTab::CreateStatic(
			[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(LOCTEXT("FMixerInsightEditorTabTitleAction", "Mixer Actions"))
					[
						SNew(SMixerInsightActionView)
					];
			}));
		TabManager->RegisterTabSpawner(MixerInsightEditorTabName_JobBatches, FOnSpawnTab::CreateStatic(
			[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(LOCTEXT("FMixerInsightEditorTabTitleJobBatches", "Mixer Jobs & Batches"))
					[
						SNew(SMixerInsightSessionView)
					];
			}));
		TabManager->RegisterTabSpawner(MixerInsightEditorTabName_Resources, FOnSpawnTab::CreateStatic(
			[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(LOCTEXT("FMixerInsightEditorTabTitleResources", "Mixer Resources"))
					[
						SNew(SMixerInsightResourceView)
					]; 
			}));
		TabManager->RegisterTabSpawner(MixerInsightEditorTabName_Devices, FOnSpawnTab::CreateStatic(
			[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(LOCTEXT("FMixerInsightEditorTabTitleDevices", "Mixer Devices"))
					[
						SNew(SMixerInsightDeviceListView)
					];
			}));
		TabManager->RegisterTabSpawner(MixerInsightEditorTabName_Inspector, FOnSpawnTab::CreateStatic(
			[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(LOCTEXT("FMixerInsightEditorTabTitleAction", "Mixer Inspector"))
					[
						SNew(SMixerInsightInspectorView)
					];
			}));

		Layout = FTabManager::NewLayout("Standalone_MixerInsight_Layout_v2")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(.4f)
					->SetHideTabWell(true)
					->AddTab(MixerInsightEditorTabName_Mixes, ETabState::OpenedTab)
					->AddTab(MixerInsightEditorTabName_Actions, ETabState::OpenedTab)
					->AddTab(MixerInsightEditorTabName_JobBatches, ETabState::OpenedTab)
					->AddTab(MixerInsightEditorTabName_Resources, ETabState::OpenedTab)
					->AddTab(MixerInsightEditorTabName_Devices, ETabState::OpenedTab)
					->SetForegroundTab(MixerInsightEditorTabName_JobBatches)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(.4f)
					->SetHideTabWell(true)
					->AddTab(MixerInsightEditorTabName_Inspector, ETabState::OpenedTab)
					->SetForegroundTab(MixerInsightEditorTabName_Inspector)
				)
			);
	}

	Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout.ToSharedRef());

	TSharedRef<SWidget> TabContents = TabManager->RestoreFrom(Layout.ToSharedRef(), TSharedPtr<SWindow>()).ToSharedRef();


	// build command list for tab restoration menu:
	TSharedPtr<FUICommandList> CommandList = MakeShareable(new FUICommandList());

	TWeakPtr<FTabManager> DebuggingToolsManagerWeak = TabManager;

	const auto ToggleTabVisibility = [](TWeakPtr<FTabManager> InTabManagerWeak, FName InTabName)
	{
		TSharedPtr<FTabManager> InDebuggingToolsManager = InTabManagerWeak.Pin();
		if (InDebuggingToolsManager.IsValid())
		{
			TSharedPtr<SDockTab> ExistingTab = InDebuggingToolsManager->FindExistingLiveTab(InTabName);
			if (ExistingTab.IsValid())
			{
				ExistingTab->RequestCloseTab();
			}
			else
			{
				InDebuggingToolsManager->TryInvokeTab(InTabName);
			}
		}
	};

	const auto IsTabVisible = [](TWeakPtr<FTabManager> InTabManagerWeak, FName InTabName)
	{
		TSharedPtr<FTabManager> InDebuggingToolsManager = InTabManagerWeak.Pin();
		if (InDebuggingToolsManager.IsValid())
		{
			return InDebuggingToolsManager->FindExistingLiveTab(InTabName).IsValid();
		}
		return false;
	};

	const auto ActionMapperToCommandList = [&](TSharedPtr<FUICommandInfo> command, const FName name)
	{
		CommandList->MapAction(
			command,
			FExecuteAction::CreateStatic(
				ToggleTabVisibility,
				DebuggingToolsManagerWeak,
				name
			),
			FCanExecuteAction::CreateStatic(
				[]() { return true; }
			),
			FIsActionChecked::CreateStatic(
				IsTabVisible,
				DebuggingToolsManagerWeak,
				name
			)
		);
	};

	ActionMapperToCommandList(FInsightTabCommands::Get().ShowMixesTab, MixerInsightEditorTabName_Mixes);
	ActionMapperToCommandList(FInsightTabCommands::Get().ShowActionsTab, MixerInsightEditorTabName_Actions);
	ActionMapperToCommandList(FInsightTabCommands::Get().ShowJobBatchesTab, MixerInsightEditorTabName_JobBatches);
	ActionMapperToCommandList(FInsightTabCommands::Get().ShowResourcesTab, MixerInsightEditorTabName_Resources);
	ActionMapperToCommandList(FInsightTabCommands::Get().ShowDevicesTab, MixerInsightEditorTabName_Devices);
	ActionMapperToCommandList(FInsightTabCommands::Get().ShowInspectorTab, MixerInsightEditorTabName_Inspector);


	TWeakPtr<SWidget> OwningWidgetWeak = NomadTab;
	TabContents->SetOnMouseButtonUp(
		FPointerEventHandler::CreateStatic(
			[]( /** The geometry of the widget*/
				const FGeometry&,
				/** The Mouse Event that we are processing */
				const FPointerEvent& PointerEvent,
				TWeakPtr<SWidget> InOwnerWeak,
				TSharedPtr<FUICommandList> InCommandList) -> FReply
			{
				if (PointerEvent.GetEffectingButton() == EKeys::RightMouseButton)
				{
					// if the tab manager is still available then make a context window that allows users to
					// show and hide tabs:
					TSharedPtr<SWidget> InOwner = InOwnerWeak.Pin();
					if (InOwner.IsValid())
					{
						FMenuBuilder MenuBuilder(true, InCommandList);

						MenuBuilder.PushCommandList(InCommandList.ToSharedRef());
						{
							MenuBuilder.AddMenuEntry(FInsightTabCommands::Get().ShowMixesTab);
							MenuBuilder.AddMenuEntry(FInsightTabCommands::Get().ShowActionsTab);
							MenuBuilder.AddMenuEntry(FInsightTabCommands::Get().ShowJobBatchesTab);
							MenuBuilder.AddMenuEntry(FInsightTabCommands::Get().ShowResourcesTab);
							MenuBuilder.AddMenuEntry(FInsightTabCommands::Get().ShowDevicesTab);
							MenuBuilder.AddMenuEntry(FInsightTabCommands::Get().ShowInspectorTab);
						}
						MenuBuilder.PopCommandList();

						FWidgetPath WidgetPath = PointerEvent.GetEventPath() != nullptr ? *PointerEvent.GetEventPath() : FWidgetPath();
						FSlateApplication::Get().PushMenu(InOwner.ToSharedRef(), WidgetPath, MenuBuilder.MakeWidget(), PointerEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

						return FReply::Handled();
					}
				}

				return FReply::Unhandled();
			}
			, OwningWidgetWeak
			, CommandList
			)
	);

	NomadTab->SetContent(
		SNew(SBorder)
		[
			TabContents
		]
	);

	return NomadTab;
}

void FMixerInsightEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(MixerInsightEditorTabName);
}

void FMixerInsightEditorModule::RegisterMenus()
{
	// // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	// FToolMenuOwnerScoped OwnerScoped(this);
	//
	// {
	// 	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	// 	{
	// 		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
	// 		Section.AddMenuEntryWithCommandList(FMixerInsightEditorCommands::Get().OpenPluginWindow, Commands);
	// 	}
	// }
	//
	// {
	// 	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
	// 	{
	// 		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
	// 		{
	// 			FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FMixerInsightEditorCommands::Get().OpenPluginWindow));
	// 			Entry.SetCommandList(Commands);
	// 		}
	// 	}
	// }
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMixerInsightEditorModule, MixerInsightEditor)
