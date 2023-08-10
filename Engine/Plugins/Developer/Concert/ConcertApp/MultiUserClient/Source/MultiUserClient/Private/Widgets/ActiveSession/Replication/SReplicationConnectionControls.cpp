// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationConnectionControls.h"

#include "IConcertSyncClient.h"
#include "IConcertSyncClientModule.h"
#include "Customization/ShowJoinSettingsCustomization.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Settings/MultiUserReplicationSettings.h"
#include "Util/ReplicationUtils.h"

#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SNegativeActionButton.h"
#include "SPositiveActionButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#if WITH_EDITOR
#include "IMultiUserReplicationEditorModule.h"
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "SReplicationConnectionControls"

namespace UE::MultiUserClient
{
	void SReplicationConnectionControls::Construct(const FArguments& InArgs)
	{
		TSharedPtr<SVerticalBox> Content;
		ChildSlot
		[
			SAssignNew(Content, SVerticalBox)

			// Buttons
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SNew(SHorizontalBox)

				// Join
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SPositiveActionButton)
					.Text(LOCTEXT("Join.Label", "Join"))
					.ToolTipText(this, &SReplicationConnectionControls::GetJoinButtonTooltip)
					.OnClicked(this, &SReplicationConnectionControls::OnJoinButtonClicked)
					.IsEnabled(this, &SReplicationConnectionControls::IsJoinButtonEnabled)
					.Visibility(this, &SReplicationConnectionControls::GetJoinButtonVisibility)
				]

				// Leave
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SNegativeActionButton)
					.ActionButtonStyle(EActionButtonStyle::Warning)
					.Text(LOCTEXT("Leave.Label", "Leave"))
					.ToolTipText(LOCTEXT("Leave.Description", "Leaves the replication session. Will stop sending and receiving replication data. You will stay in the Concert session, i.e. continue to receive transaction events, etc."))
					.OnClicked(this, &SReplicationConnectionControls::OnLeaveButtonClicked)
					.Visibility(this, &SReplicationConnectionControls::GetLeaveButtonVisibility)
				]

				// The "Settings" icons.
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				.VAlign(VAlign_Fill)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
					.Visibility(this, &SReplicationConnectionControls::GetCogWheelSettingsVisibility)
					.OnClicked_Lambda([]()
					{
						using namespace UE::MultiUserReplicationEditor;
						IMultiUserReplicationEditorModule& ReplicationEditorModule = IMultiUserReplicationEditorModule::Get();
						const IMultiUserReplicationEditorModule::FSettingPath ReplicationSettings = ReplicationEditorModule.GetReplicationSettingsInfo();
						FModuleManager::GetModulePtr<ISettingsModule>("Settings")->ShowViewer(ReplicationSettings.ContainerName, ReplicationSettings.CategoryName, ReplicationSettings.SectionName);
						return FReply::Handled();
					})
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Settings"))
					]
				]
			]
		];

#if WITH_EDITOR
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ColumnWidth = 0.5f;
		
		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		DetailsView->RegisterInstancedCustomPropertyLayout(UMultiUserReplicationSettings::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FShowJoinSettingsCustomization::Make));
		DetailsView->SetObject(GetMutableDefault<UMultiUserReplicationSettings>());
		DetailsView->SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SReplicationConnectionControls::GetOverrideSettingsVisibility));
		
		Content->AddSlot()
		.AutoHeight()
		[
			DetailsView
		];
#endif
	}

	UMultiUserReplicationClientProfileAsset* SReplicationConnectionControls::GetClientProfile() const
	{
		return GetMutableDefault<UMultiUserReplicationSettings>()->DefaultSessionSettings.DefaultProfile.LoadSynchronous();
	}

	FReply SReplicationConnectionControls::OnJoinButtonClicked()
	{
		TSharedPtr<IConcertSyncClient> Client = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
		IConcertClientReplicationManager* ReplicationManager = Client ? Client->GetReplicationManager() : nullptr;
		if (!ensure(ReplicationManager))
		{
			return FReply::Handled();
		}

		UMultiUserReplicationClientProfileAsset* ClientProfile = GetClientProfile();
		checkf(ClientProfile, TEXT("Validate IsJoinButtonEnabled implementation!"));
		// All the UI callbacks for visibility, etc. will update automatically since they do polling.
		ReplicationUtils::JoinSessionWithEditorNotifications(*ReplicationManager, ClientProfile->ToJoinArgs());
		return FReply::Handled();
	}

	FText SReplicationConnectionControls::GetJoinButtonTooltip() const
	{
		FText ErrorReason = LOCTEXT("Join.Tooltip", "Attempts to join a replication session with the server to start sending and receive replication data.");
		IsJoinButtonEnabled(&ErrorReason);
		return ErrorReason;
	}

	bool SReplicationConnectionControls::IsJoinButtonEnabled(FText* ErrorReason) const
	{
		const bool bHasValidAsset = GetClientProfile() != nullptr;
		if (!bHasValidAsset && ErrorReason)
		{
			*ErrorReason = LOCTEXT("IsJoinButtonEnabled.Error.InvalidProfile", "No client profile selected");
			return false;
		}
		
		TSharedPtr<IConcertSyncClient> Client = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
		IConcertClientReplicationManager* ReplicationManager = Client ? Client->GetReplicationManager() : nullptr;
		const bool bCanMakeJoinRequest = ReplicationManager && ReplicationManager->CanJoin();
		if (!bCanMakeJoinRequest && ErrorReason)
		{
			*ErrorReason = LOCTEXT("IsJoinButtonEnabled.Error.JoinRequestInProgress", "A join request is in progress.");
			return false;
		}
		
		return bHasValidAsset && bCanMakeJoinRequest;
	}

	EVisibility SReplicationConnectionControls::GetJoinButtonVisibility() const
	{
		TSharedPtr<IConcertSyncClient> Client = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
		IConcertClientReplicationManager* ReplicationManager = Client ? Client->GetReplicationManager() : nullptr;
		const bool bShouldBeVisible = !ReplicationManager || !ReplicationManager->IsConnectedToReplicationSession();
		return bShouldBeVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FReply SReplicationConnectionControls::OnLeaveButtonClicked()
	{
		TSharedPtr<IConcertSyncClient> Client = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
		IConcertClientReplicationManager* ReplicationManager = Client ? Client->GetReplicationManager() : nullptr;
		if (ensure(ReplicationManager))
		{
			ReplicationUtils::LeaveSessionWithEditorNotifications(*ReplicationManager);
		}
		return FReply::Handled();
	}
}

#undef LOCTEXT_NAMESPACE
