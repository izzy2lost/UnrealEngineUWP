// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationJoinedToolbar.h"

#include "ConcertLogGlobal.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/Stream/ClientStreamRepository.h"
#include "Replication/Stream/LocalClientStreamSynchronizer.h"

#include "Framework/Notifications/NotificationManager.h"
#include "SNegativeActionButton.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicationJoinedWidget"

namespace UE::MultiUserClient
{
	void SReplicationJoinedToolbar::Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager)
	{
		ReplicationManager = InReplicationManager;
		
		ChildSlot
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]

			// Upload button
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				BuildUploadButton()
			]

			// Revert changes
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				BuildRevertButton()
			]
		];
	}

	TSharedRef<SWidget> SReplicationJoinedToolbar::BuildUploadButton()
	{
		return SNew(SButton)
			.ForegroundColor(FSlateColor::UseStyle())
			.IsEnabled_Lambda([this](){ return ReplicationManager->GetStreamSynchronizer()->GetDiffer().CanSubmitChanges(); })
			.ToolTipText(this, &SReplicationJoinedToolbar::GetUploadButtonToolTipText)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SReplicationJoinedToolbar::OnUploadButtonClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image_Lambda([this]()
					{
						const FLocalClientStreamSynchronizer& Differ = ReplicationManager->GetStreamSynchronizer()->GetDiffer();
						return Differ.HasAnyWarnings()
							? FAppStyle::Get().GetBrush("Icons.Warning")
							: FAppStyle::Get().GetBrush("Icons.Plus");
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const FLocalClientStreamSynchronizer& Differ = ReplicationManager->GetStreamSynchronizer()->GetDiffer();
						return Differ.HasAnyWarnings()
							? FStyleColors::AccentYellow
							: FStyleColors::AccentGreen;
					})
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
					.Text(LOCTEXT("UploadChanges.Label", "Upload"))
				]
			];
	}

	FText SReplicationJoinedToolbar::GetUploadButtonToolTipText() const
	{
		const FLocalClientStreamSynchronizer& Differ = ReplicationManager->GetStreamSynchronizer()->GetDiffer();
		const FText BaseToolTipText = [this, &Differ]()
		{
			if (Differ.HasSubmittableLocalChanges() && !Differ.IsChangeRequestInTransit())
			{
				return LOCTEXT("UploadChanges.ToolTip.Upload", "Upload your local changes to the server.");
			}
			if (Differ.IsChangeRequestInTransit())
			{
				return LOCTEXT("UploadChanges.ToolTip.InTransit", "Awaiting server response.");
			}
			return LOCTEXT("UploadChanges.ToolTip.NoChanges", "No submittable changes.");
		}();
		if (!Differ.HasAnyWarnings())
		{
			return BaseToolTipText;
		}

		return FText::Format(LOCTEXT("UploadChanges.ToolTip.BaseFmt", "{0}\n\n{1}"),
			BaseToolTipText,
			LOCTEXT("UploadChanges.ToolTip", "Warning: Objects with warnings will not be submitted. There are objects with warnings.")
			);
	}

	FReply SReplicationJoinedToolbar::OnUploadButtonClicked() const
	{
		FLocalClientStreamSynchronizer& Differ = ReplicationManager->GetStreamSynchronizer()->GetDiffer();
		if (Differ.CanSubmitChanges())
		{
			FNotificationInfo NotificationInfo(LOCTEXT("Uploading.InProgress", "Uploading stream changes."));
			NotificationInfo.bUseThrobber = true;
			NotificationInfo.bUseSuccessFailIcons = true;
			NotificationInfo.bFireAndForget = false;
			const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
			Differ.SubmitChanges()
				.Next([Notification](FLocalClientStreamSynchronizer::FSubmissionResult&& Result)
				{
					const ConcertSyncClient::Replication::FChangeStreamResponse& Response = Result.Value;
					if (Response.IsSuccess())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Success);
						Notification->SetText(LOCTEXT("Uploading.Success", "Changes accepted."));
									
						const ConcertSyncClient::Replication::FChangeStreamRequest& Request = Result.Key;
						// Multi-User adds only one stream so we only need to look at index 0
						const int32 NumAdditions = Request.StreamsToAdd.IsEmpty() ? 0 : Request.StreamsToAdd[0].BaseDescription.ReplicationMap.ReplicatedObjects.Num();
						const int32 NumPuts = Request.ObjectsToPut.Num() + NumAdditions;
						Notification->SetSubText(FText::Format(LOCTEXT("Uploading.Success.SubText", "{0} puts, {1} removals"),
							NumPuts,
							Request.ObjectsToRemove.Num()
							));
					}
					else
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
						Notification->SetText(LOCTEXT("Uploading.Failure", "Changes rejected."));
						Notification->SetSubText(LOCTEXT("Uploading.Failure.SubText", "See log for details."));

						FStringOutputDevice Errors;
						Response.LogErrors(Errors);
						UE_LOG(LogConcert, Error, TEXT("Errors uploading stream changes:\n%s"), *Errors);
					}
								
					Notification->ExpireAndFadeout();
				});
		}
		return FReply::Handled();
	}

	TSharedRef<SWidget> SReplicationJoinedToolbar::BuildRevertButton()
	{
		return SNew(SNegativeActionButton)
			.Text(LOCTEXT("RevertChanges.Label", "Revert"))
			.ToolTipText_Lambda([this]()
			{
				const FLocalClientStreamSynchronizer& Differ = ReplicationManager->GetStreamSynchronizer()->GetDiffer();
				return Differ.HasRevertableLocalChanges()
					? LOCTEXT("RevertChanges.ToolTip.Revert", "Reverts all local changes you've made to the state that's on the server.")
					: LOCTEXT("RevertChanges.ToolTip.NothingToUpload", "Once you make changes to the stream, you can use this button to revert them.");
			})
			.IsEnabled_Lambda([this](){ return ReplicationManager->GetStreamSynchronizer()->GetDiffer().HasRevertableLocalChanges(); })
			.OnClicked_Lambda([this]()
			{
				FLocalClientStreamSynchronizer& Differ = ReplicationManager->GetStreamSynchronizer()->GetDiffer();
				Differ.RevertCachedChanges();
				return FReply::Handled();
			});
	}
}

#undef LOCTEXT_NAMESPACE