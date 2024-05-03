// Copyright Epic Games, Inc. All Rights Reserved.

#include "SActiveSessionRoot.h"

#include "Overview/SActiveSessionOverviewTab.h"
#include "Replication/SReplicationRootWidget.h"
#include "SActiveSessionToolbar.h"

#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SActiveSessionRoot"

namespace UE::MultiUserClient
{
	const FName SActiveSessionRoot::SessionOverviewTabId(TEXT("OverviewTabId"));
	const FName SActiveSessionRoot::ReplicationTabId(TEXT("ReplicationTabId"));
	
	void SActiveSessionRoot::Construct(
		const FArguments& InArgs,
		const TSharedRef<SDockTab>& ConstructUnderMajorTab,
		TSharedPtr<IConcertSyncClient> InConcertSyncClient,
		TSharedRef<FMultiUserReplicationManager> InReplicationManager
		)
	{
		ConcertSyncClient = InConcertSyncClient;
		
		TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);
		TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("ConcertActiveSession", "Active Session"));
		TabManager->SetAllowWindowMenuBar(true);
		RegisterTabSpawners(TabManager.ToSharedRef(), AppMenuGroup, InReplicationManager);

		// Create our content
		const TSharedRef<FTabManager::FLayout> Layout =
			FTabManager::NewLayout("ConcertActiveSession_Layout_v3")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->Split
				(
					FTabManager::NewStack()
					->AddTab(SessionOverviewTabId, ETabState::OpenedTab)
					->AddTab(ReplicationTabId, ETabState::OpenedTab)
					->SetForegroundTab(SessionOverviewTabId)
					->SetHideTabWell(false)
					)
			);
		
		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.0f)
				[
					SNew(SActiveSessionToolbar, InConcertSyncClient)
				]
			]

			// Tabs
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				TabManager->RestoreFrom(Layout, nullptr).ToSharedRef()
			]
		];
	}

	void SActiveSessionRoot::RegisterTabSpawners(
		const TSharedRef<FTabManager>& InTabManager,
		const TSharedRef<FWorkspaceItem>& AppMenuGroup,
		TSharedRef<FMultiUserReplicationManager> InReplicationManager
		)
	{
		InTabManager->RegisterTabSpawner(SessionOverviewTabId, FOnSpawnTab::CreateSP(this, &SActiveSessionRoot::SpawnTab_SessionOverview))
			.SetDisplayName(LOCTEXT("OverviewTab.DisplayName", "Overview"))
			.SetGroup(AppMenuGroup);
	
		InTabManager->RegisterTabSpawner(ReplicationTabId, FOnSpawnTab::CreateSP(this, &SActiveSessionRoot::SpawnTab_ReplicationControls, InReplicationManager))
			.SetDisplayName(LOCTEXT("ReplicationTab.DisplayName", "Replication"))
			.SetGroup(AppMenuGroup);
	}

	TSharedRef<SDockTab> SActiveSessionRoot::SpawnTab_SessionOverview(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.Label(LOCTEXT("SessionOverviewTab.Label", "Session"))
			.ToolTipText(LOCTEXT("SessionOverviewTab.Tooltip", "Displays active session clients and activity."))
			[
				SNew(SActiveSessionOverviewTab, ConcertSyncClient)
			];
	}

	TSharedRef<SDockTab> SActiveSessionRoot::SpawnTab_ReplicationControls(const FSpawnTabArgs& Args, TSharedRef<FMultiUserReplicationManager> InReplicationManager)
	{
		return SNew(SDockTab)
			.Label(LOCTEXT("ReplicationTab.Label", "Replication"))
			.ToolTipText(LOCTEXT("ReplicationTab.Tooltip", "Manage real-time object replication"))
			[
				SNew(SReplicationRootWidget, InReplicationManager, ConcertSyncClient.ToSharedRef())
			];
	}
}

#undef LOCTEXT_NAMESPACE