// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDMainTab.h"

#include "ChaosVDEditorVisualizationSettingsTab.h"
#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
#include "ChaosVDObjectDetailsTab.h"
#include "ChaosVDOutputLogTab.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportTab.h"
#include "ChaosVDSolversTracksTab.h"
#include "ChaosVDStyle.h"
#include "ChaosVDTabsIDs.h"
#include "ChaosVDWorldOutlinerTab.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SWindowTitleBar.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "StatusBarSubsystem.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"
#include "Trace/ChaosVDTraceManager.h"
#include "Widgets/SChaosBrowseTraceFileSourceModal.h"
#include "Widgets/SChaosVDBrowseSessionsModal.h"
#include "Widgets/SChaosVDRecordingControls.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"


void SChaosVDMainTab::Construct(const FArguments& InArgs, TSharedPtr<FChaosVDEngine> InChaosVDEngine)
{
	ChaosVDEngine = InChaosVDEngine;

	TabManager = FGlobalTabmanager::Get()->NewTabManager(InArgs._OwnerTab.ToSharedRef()).ToSharedPtr();

	// Create the UI Tabs Handlers
	//TODO: Pass self as a weakPtr
	WorldOutlinerTab = MakeShared<FChaosVDWorldOutlinerTab>(FChaosVDTabID::WorldOutliner, TabManager, this);
	ObjectDetailsTab = MakeShared<FChaosVDObjectDetailsTab>(FChaosVDTabID::DetailsPanel, TabManager, this);
	OutputLogTab = MakeShared<FChaosVDOutputLogTab>(FChaosVDTabID::OutputLog, TabManager, this);
	PlaybackViewportTab = MakeShared<FChaosVDPlaybackViewportTab>(FChaosVDTabID::PlaybackViewport, TabManager, this);
	SolversTracksTab = MakeShared<FChaosVDSolversTracksTab>(FChaosVDTabID::SolversTrack, TabManager, this);
	EditorSettingsTab = MakeShared<FChaosVDEditorVisualizationSettingsTab>(FChaosVDTabID::CVDEditorSettings, TabManager, this);

	StatusBarID = FName(FChaosVDTabID::StatusBar.ToString() + InChaosVDEngine->GetInstanceGuid().ToString());
	
	UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr;
	check(StatusBarSubsystem);
	TSharedRef<SWidget> StatusBarWidget = StatusBarSubsystem->MakeStatusBarWidget(StatusBarID, TabManager->GetOwnerTab().ToSharedRef());

	// Status bars come with the output log and content browser drawers by default, therefore we need to remove them otherwise they will be on the tool's window
	StatusBarSubsystem->UnregisterDrawer(StatusBarID, "ContentBrowser");
	StatusBarSubsystem->UnregisterDrawer(StatusBarID, "OutputLog");
	
	GenerateMainWindowMenu();

	ChildSlot
	[
		// Row between the tab and main content 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBorder )
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
			[
				// Open Button
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(FMargin(12, 7, 6, 7))
				.AutoWidth()
				[
					SNew(SButton)
					.ToolTip(SNew(SToolTip).Text(LOCTEXT("OpenFileDesc", "Click here to open a Chaos Visual Debugger file.")))
					.ContentPadding(FMargin(0, 5.f, 0, 4.f))
					.OnClicked_Lambda([this]()
					{
						BrowseAndOpenChaosVDRecording();
						return FReply::Handled();
					})
					.Content()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
							.ColorAndOpacity(FStyleColors::AccentGreen)
						]
						+SHorizontalBox::Slot()
						.Padding(FMargin(3, 0, 0, 0))
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "SmallButtonText")
							.Text(LOCTEXT("OpenFile", "Open File"))
						]
					]
				]

				// Remote Connection Button
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(FMargin(6, 7, 18, 7))
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(0, 5.f, 0, 4.f))
					.OnClicked_Raw(this, &SChaosVDMainTab::HandleSessionConnectionClicked)
					.Content()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FChaosVDStyle::Get().GetBrush("ConnectionIcon"))
							.ColorAndOpacity(FStyleColors::AccentGreen)
						]
						+SHorizontalBox::Slot()
						.Padding(FMargin(3, 0, 0, 0))
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "SmallButtonText")
							.Text_Raw(this, &SChaosVDMainTab::GetConnectButtonText)
						]
					]
				]


				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1.0f)
				[
					SNew(SSeparator)
						.Orientation(Orient_Vertical)
						.Thickness(2.0f)
						.ColorAndOpacity(FColor::Black)
						.SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
				]
				
				+SHorizontalBox::Slot()
				[
					
					SNew(SChaosVDRecordingControls, StaticCastWeakPtr<SChaosVDMainTab>(AsWeak()))
				]
				
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1.0f, 5.0f, 1.0f, 5.0f)
				[
					SNew(SSeparator)
						.Orientation(Orient_Vertical)
						.Thickness(2.0f)
						.ColorAndOpacity(FColor::Black)
						.SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
				]

				// Settings button
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(14.f, 0.f, 14.f, 0.f))
				[
					SNew(SComboButton)
					.ContentPadding(0)
					.HasDownArrow(false)
					.IsEnabled(false) // Disabled until we create content for it
					.ForegroundColor(FSlateColor::UseForeground())
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
					/*.MenuContent()
					[
						//TODO: Implement Settings menu
					]*/
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Toolbar.Settings"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
						+ SHorizontalBox::Slot()
						.Padding(FMargin(5, 0, 0, 0))
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "NormalText")
							.Text(LOCTEXT("SettingsLabel", "Settings"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]
			]
				
		]
		
		// Main Visual Debugger Interface content
		+SVerticalBox::Slot()
		.Padding(FMargin(0.0f,5.0f,0.0f,0.0f))
		[
			TabManager->RestoreFrom(GenerateMainLayout(), TabManager->GetOwnerTab()->GetParentWindow()).ToSharedRef()
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			StatusBarWidget
		]
	];

	TabManager->TryInvokeTab(FChaosVDTabID::SolversTrack);
}

TSharedRef<FTabManager::FLayout> SChaosVDMainTab::GenerateMainLayout()
{
	return FTabManager::NewLayout("ChaosVisualDebugger_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->SetExtensionId("TopLevelArea")
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.8f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(FChaosVDTabID::PlaybackViewport, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(FChaosVDTabID::SolversTrack, ETabState::OpenedTab)
					->AddTab(FChaosVDTabID::OutputLog, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.15f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(FChaosVDTabID::WorldOutliner, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(FChaosVDTabID::DetailsPanel, ETabState::OpenedTab)
					->AddTab(FChaosVDTabID::CVDEditorSettings, ETabState::ClosedTab)
				)
			)
		);
}

void SChaosVDMainTab::GenerateMainWindowMenu()
{
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList >());
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenuLabel", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(LOCTEXT("OpenFileMenuLabel", "OpenFile"), FText(), FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					BrowseAndOpenChaosVDRecording();
				})));
		}),
		"File"
	);
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
		}),
		"Window"
	);

	TabManager->SetAllowWindowMenuBar(true);
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuBarBuilder.MakeWidget());
}

void SChaosVDMainTab::BrowseAndOpenChaosVDRecording()
{
	const TSharedRef<SChaosBrowseTraceFileSourceModal> SessionBrowserModal = SNew(SChaosBrowseTraceFileSourceModal);

	EChaosVDBrowseFileModalResponse Response = SessionBrowserModal->ShowModal();
	switch(Response)
	{
		case EChaosVDBrowseFileModalResponse::OpenFolder:
			{
				BrowseChaosVDRecordingFromFolder();
				break;
			}
		case EChaosVDBrowseFileModalResponse::OpenTraceStore:
			{
				//TODO: Support remote Trace Stores 
				BrowseChaosVDRecordingFromFolder(FChaosVDModule::Get().GetTraceManager()->GetLocalTraceStoreDirPath());
				break;
			}
		case EChaosVDBrowseFileModalResponse::Cancel:
			break;
		default:
				ensureMsgf(false, TEXT("Invalid responce received"));
			break;
	}
}

void SChaosVDMainTab::BrowseChaosVDRecordingFromFolder(FStringView FolderPath)
{
	TArray<FString> OutOpenFilenames;
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("Unreal Trace|*.utrace|");
		//TODO: Re-enable this when we add "Clips" support as these will use our own format
		//ExtensionStr += TEXT("Chaos Visual Debugger|*.cvd");
	
		DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			LOCTEXT("OpenDialogTitle", "Open Chaos Visual Debug File").ToString(),
			FolderPath.GetData(),
			TEXT(""),
			*ExtensionStr,
			EFileDialogFlags::None,
			OutOpenFilenames
		);
	}

	if (OutOpenFilenames.Num() > 0)
	{
		if (OutOpenFilenames[0].EndsWith(TEXT("utrace")))
		{
			GetChaosVDEngineInstance()->LoadRecording(OutOpenFilenames[0]);
		}
	}
}

void SChaosVDMainTab::BrowseLiveSessionsFromTraceStore() const
{
	const TSharedRef<SChaosVDBrowseSessionsModal> SessionBrowserModal = SNew(SChaosVDBrowseSessionsModal);

	if (SessionBrowserModal->ShowModal() != EAppReturnType::Cancel)
	{
		bool bSuccess = false;
		const FChaosVDTraceSessionInfo SessionInfo = SessionBrowserModal->GetSelectedTraceInfo();
		if (SessionInfo.bIsValid)
		{
			FChaosVDTraceSessionDescriptor NewSessionFromFileDescriptor;
			NewSessionFromFileDescriptor.SessionName =  FChaosVDModule::Get().GetTraceManager()->ConnectToLiveSession(SessionBrowserModal->GetSelectedTraceStoreAddress(), SessionInfo.TraceID);
			NewSessionFromFileDescriptor.bIsLiveSession = true;

			bSuccess = !NewSessionFromFileDescriptor.SessionName.IsEmpty();

			// If it failed we want to clean the current session name, so it is ok calling it either way
			GetChaosVDEngineInstance()->SetCurrentSession(NewSessionFromFileDescriptor);
		}

		if (!bSuccess)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToConnectToSessionMessage", "Failed to connect to session"));	
		}
	}
}

FReply SChaosVDMainTab::HandleSessionConnectionClicked()
{
	if (TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr =  GetChaosVDEngineInstance()->GetPlaybackController())
	{
		const bool bIsAlreadyInLiveSession = PlaybackControllerPtr->IsPlayingLiveSession();
	
		if (bIsAlreadyInLiveSession)
		{
			FChaosVDModule::Get().GetTraceManager()->CloseSession(GetChaosVDEngineInstance()->GetCurrentSessionDescriptor().SessionName);
			PlaybackControllerPtr->HandleDisconnectedFromSession();
		}
		else
		{
			BrowseLiveSessionsFromTraceStore();
		}
	}

	return FReply::Handled();
}

FText SChaosVDMainTab::GetConnectButtonText() const
{
	const bool bIsAlreadyInLiveSession = GetChaosVDEngineInstance()->GetPlaybackController()->IsPlayingLiveSession();
	return bIsAlreadyInLiveSession ? LOCTEXT("DisconnectFromSession", "Disconnect from Session") : LOCTEXT("ConnectToSession", "Connect to Session");
}

#undef LOCTEXT_NAMESPACE
