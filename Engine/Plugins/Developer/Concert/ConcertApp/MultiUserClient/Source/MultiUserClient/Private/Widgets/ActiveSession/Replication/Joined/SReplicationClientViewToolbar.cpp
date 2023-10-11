// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationClientViewToolbar.h"

#include "ConcertLogGlobal.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/Stream/LocalStreamChangeTracker.h"

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
	void SReplicationClientViewToolbar::Construct(const FArguments& InArgs)
	{
		GetChangeTrackerAttribute = InArgs._GetChangeTrackerAttribute;
		check(GetChangeTrackerAttribute.IsBound() || GetChangeTrackerAttribute.IsSet());
		
		ChildSlot
		[
			SNew(SHorizontalBox)
			
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(30.f, 0.f)
			[
				InArgs._AdditionalToolbarWidgets.Widget
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

	FLocalStreamChangeTracker& SReplicationClientViewToolbar::GetChangeTracker() const
	{
		FLocalStreamChangeTracker* ChangeTracker = GetChangeTrackerAttribute.Get();
		checkf(ChangeTracker, TEXT("You were supposed to destroy this widget."));
		return *ChangeTracker;
	}

	TSharedRef<SWidget> SReplicationClientViewToolbar::BuildUploadButton()
	{
		return SNew(SButton)
			.ForegroundColor(FSlateColor::UseStyle())
			.IsEnabled_Lambda([this](){ return GetChangeTracker().CanSubmitChanges(); })
			.ToolTipText(this, &SReplicationClientViewToolbar::GetUploadButtonToolTipText)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SReplicationClientViewToolbar::OnUploadButtonClicked)
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
						const FLocalStreamChangeTracker& Differ = GetChangeTracker();
						return Differ.HasAnyWarnings()
							? FAppStyle::Get().GetBrush("Icons.Warning")
							: FAppStyle::Get().GetBrush("Icons.Plus");
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const FLocalStreamChangeTracker& Differ = GetChangeTracker();
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

	FText SReplicationClientViewToolbar::GetUploadButtonToolTipText() const
	{
		const FLocalStreamChangeTracker& Differ = GetChangeTracker();
		const FText BaseToolTipText = [this, &Differ]()
		{
			if (Differ.HasSubmittableLocalChanges() && !Differ.CanMakeSubmitNetworkRequest())
			{
				return LOCTEXT("UploadChanges.ToolTip.Upload", "Upload your local changes to the server.");
			}
			if (Differ.CanMakeSubmitNetworkRequest())
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

	FReply SReplicationClientViewToolbar::OnUploadButtonClicked() const
	{
		FLocalStreamChangeTracker& Differ = GetChangeTracker();
		if (Differ.CanSubmitChanges())
		{
			FNotificationInfo NotificationInfo(LOCTEXT("Uploading.InProgress", "Uploading stream changes."));
			NotificationInfo.bUseThrobber = true;
			NotificationInfo.bUseSuccessFailIcons = true;
			NotificationInfo.bFireAndForget = false;
			const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
			Differ.SubmitChanges()
				.Next([Notification](FSubmitChangesResult&& SubmitChangesResult)
				{
					TOptional<FCompletedChangeSubmission> SubmissionInfo = SubmitChangesResult.SubmissionInfo;
					if (SubmissionInfo.IsSet() && SubmissionInfo->Response.IsSuccess())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Success);
						Notification->SetText(LOCTEXT("Uploading.Success", "Changes accepted."));
									
						const ConcertSyncClient::Replication::FChangeStreamRequest& Request = SubmissionInfo->Request;
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

						if (SubmissionInfo.IsSet())
						{
							FStringOutputDevice Errors;
							SubmissionInfo->Response.LogErrors(Errors);
							UE_LOG(LogConcert, Error, TEXT("Errors uploading stream changes:\n%s"), *Errors);
						}
						else
						{
							UE_LOG(LogConcert, Error, TEXT("Failed to submit changes. ESubmitChangesErrorCode: %d"), SubmitChangesResult.ErrorCode);
						}
					}
								
					Notification->ExpireAndFadeout();
				});
		}
		return FReply::Handled();
	}

	TSharedRef<SWidget> SReplicationClientViewToolbar::BuildRevertButton()
	{
		return SNew(SNegativeActionButton)
			.Text(LOCTEXT("RevertChanges.Label", "Revert"))
			.ToolTipText_Lambda([this]()
			{
				const FLocalStreamChangeTracker& Differ = GetChangeTracker();
				return Differ.HasRevertableLocalChanges()
					? LOCTEXT("RevertChanges.ToolTip.Revert", "Reverts all local changes you've made to the state that's on the server.")
					: LOCTEXT("RevertChanges.ToolTip.NothingToUpload", "Once you make changes to the stream, you can use this button to revert them.");
			})
			.IsEnabled_Lambda([this](){ return GetChangeTracker().HasRevertableLocalChanges(); })
			.OnClicked_Lambda([this]()
			{
				FLocalStreamChangeTracker& Differ = GetChangeTracker();
				Differ.RevertCachedChanges();
				return FReply::Handled();
			});
	}
}

#undef LOCTEXT_NAMESPACE