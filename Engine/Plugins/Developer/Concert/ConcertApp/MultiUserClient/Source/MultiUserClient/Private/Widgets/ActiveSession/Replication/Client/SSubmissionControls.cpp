// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSubmissionControls.h"

#include "ConcertLogGlobal.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Replication/Submission/Data/EChangeRevertability.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopeExit.h"
#include "SNegativeActionButton.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSubmissionControls"

namespace UE::MultiUserClient
{
	void SSubmissionControls::Construct(const FArguments& InArgs)
	{
		SubmissionWorkflowAttribute = InArgs._SubmissionWorkflow;
		check(SubmissionWorkflowAttribute.IsBound() || SubmissionWorkflowAttribute.IsSet());
		
		ChildSlot
		[
			SNew(SHorizontalBox)
			
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

	TSharedRef<SWidget> SSubmissionControls::BuildUploadButton()
	{
		return SNew(SButton)
			.ForegroundColor(FSlateColor::UseStyle())
			.IsEnabled_Lambda([this](){ return GetSubmissionWorkflow().CanSubmit(); })
			.ToolTipText(this, &SSubmissionControls::GetUploadButtonToolTipText)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SSubmissionControls::OnUploadButtonClicked)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this](){ return GetSubmissionWorkflow().GetUploadability() == EChangeUploadability::InProgress ? 0 : 1; })
				
				// InProgress
				+SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SCircularThrobber)
					]
					+ SHorizontalBox::Slot()
					.Padding(FMargin(3, 0, 0, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallButtonText")
						.Text(LOCTEXT("UploadChanges.Label.Uploading", "Uploading"))
					]
				]
				// Any other EChangeUploadability
				+SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
						.ColorAndOpacity(FStyleColors::AccentGreen)
					]
					+ SHorizontalBox::Slot()
					.Padding(FMargin(3, 0, 0, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallButtonText")
						.Text(LOCTEXT("UploadChanges.Label.Upload", "Upload"))
					]
				]
			];
	}

	FText SSubmissionControls::GetUploadButtonToolTipText() const
	{
		switch(GetSubmissionWorkflow().GetUploadability())
		{
			case EChangeUploadability::Ready: return LOCTEXT("UploadChanges.ToolTip.Upload", "Upload your local changes to the server.");
			case EChangeUploadability::NoChanges: return LOCTEXT("UploadChanges.ToolTip.NoChanges", "No submittable changes.");
			case EChangeUploadability::InProgress: return LOCTEXT("UploadChanges.ToolTip.InTransit", "Awaiting server response.");
			case EChangeUploadability::NotImplemented: return LOCTEXT("UploadChanges.ToolTip.NotImplemented", "This feature is currently not implemented");
			default: checkNoEntry(); return FText::GetEmpty();
		}
	}

	FReply SSubmissionControls::OnUploadButtonClicked() const
	{
		ISubmissionWorkflow& Workflow = GetSubmissionWorkflow();
		// SubmitChanges implicitly checks whether Workflow.CanSubmit()
		if (ISubmissionOperation* SubmissionOperation = Workflow.SubmitChanges())
		{
			SetupNotificationsFor(*SubmissionOperation);
		}
		return FReply::Handled();
	}

	void SSubmissionControls::SetupNotificationsFor(ISubmissionOperation& SubmissionOperation) const
	{
		HandleStreamUpdatedNotification(SubmissionOperation);
		HandleAuthorityChangeRequestedNotification(SubmissionOperation);
	}

	void SSubmissionControls::HandleStreamUpdatedNotification(ISubmissionOperation& SubmissionOperation) const
	{
		if (!SubmissionOperation.IsModifyingStreams())
		{
			return;
		}
		
		FNotificationInfo NotificationInfo(LOCTEXT("Uploading.InProgress", "Uploading stream changes."));
		NotificationInfo.bUseThrobber = true;
		NotificationInfo.bUseSuccessFailIcons = true;
		NotificationInfo.bFireAndForget = false;
		
		const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		SubmissionOperation.OnStreamChangesSubmittedFuture()
			.Next([Notification](FSubmitStreamChangesResponse&& SubmitChangesResult)
			{
				if (SubmitChangesResult.ErrorCode == EStreamSubmissionErrorCode::Cancelled)
				{
					Notification->SetCompletionState(SNotificationItem::CS_Fail);
					Notification->SetText(LOCTEXT("Uploading.Failure.Cancelled", "Updating Stream cancelled."));
					return;
				}

				const TOptional<FCompletedChangeSubmission>& SubmissionInfo = SubmitChangesResult.SubmissionInfo;
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
					Notification->SetText(LOCTEXT("Uploading.Failure.Rejected", "Changes rejected."));
					Notification->SetSubText(LOCTEXT("Uploading.Failure.Rejected.SubText", "See log for details."));

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

	void SSubmissionControls::HandleAuthorityChangeRequestedNotification(ISubmissionOperation& SubmissionOperation) const
	{
		SubmissionOperation.OnAuthorityChangeRequestedFuture()
			.Next([WeakThis = TWeakPtr<const SSubmissionControls>(SharedThis(this))](FSubmitAuthorityChangesRequest&& RequestMetaData)
			{
				if (RequestMetaData.ErrorCode != EAuthoritySubmissionErrorCode::Success)
				{
					return;
				}

				FNotificationInfo NotificationInfo(LOCTEXT("Authority.InProgress.Text", "Requesting authority."));
				NotificationInfo.bUseThrobber = true;
				NotificationInfo.bUseSuccessFailIcons = true;
				NotificationInfo.bFireAndForget = false;
				const TSharedPtr<SNotificationItem> AuthorityChangeNotification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);

				const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request = *RequestMetaData.Request ;
				AuthorityChangeNotification->SetSubText(
					FText::Format(LOCTEXT("Authority.InProgress.SubTextFmt", "Taking {0}|plural(one=object,other\nReleasing {1}|plural(one=object,other=objects)"),
						Request.TakeAuthority.Num(),
						Request.ReleaseAuthority.Num()
					));
				
				RequestMetaData.OperationContext.OnAuthorityChangeResponseReceivedFuture()
					.Next([Request, AuthorityChangeNotification](FSubmitAuthorityChangesResponse&& Response)
					{
						HandleAuthorityChangedResponseNotification(Request, Response, AuthorityChangeNotification.ToSharedRef());
					});
			});
	}

	void SSubmissionControls::HandleAuthorityChangedResponseNotification(
		const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request,
		const FSubmitAuthorityChangesResponse& ResponseMetaData,
		const TSharedRef<SNotificationItem>& NotificationItem
		)
	{
		if (ResponseMetaData.ErrorCode == EAuthoritySubmissionErrorCode::NoChange)
		{
			return;
		}
		
		ON_SCOPE_EXIT{ NotificationItem->ExpireAndFadeout(); };
		if (ResponseMetaData.ErrorCode != EAuthoritySubmissionErrorCode::Success)
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->SetText(LOCTEXT("Authority.Failed.Text", "Authority request failed."));
			NotificationItem->SetSubText(FText::Format(LOCTEXT("Auhtority.Failed.SubText", "Error code: {0}"), LexToText(ResponseMetaData.ErrorCode)));
			return;
		}
		
		const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response = *ResponseMetaData.Response;
		
		const bool bSuccess = Response.RejectedObjects.IsEmpty();
		const FText Text = bSuccess
			? LOCTEXT("Authority.Text.Accepted", "Authority change accepted.")
			: LOCTEXT("Authority.Text.Rejection", "Authority change had rejections.");
		const FText SubText = bSuccess
			? FText::GetEmpty()
			: FText::Format(LOCTEXT("Authority.SubText.RejectionFmt", "{0} accepted {1} rejected\nSee logs for details."),
				Request.TakeAuthority.Num() - Response.RejectedObjects.Num(),
				Response.RejectedObjects.Num()
				);
				
		NotificationItem->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		NotificationItem->SetText(Text);
		NotificationItem->SetSubText(SubText);

		if (!bSuccess)
		{
			const FString Error = FString::JoinBy(Response.RejectedObjects, TEXT("\n"), [](const TPair<FSoftObjectPath, FConcertStreamArray>& Pair)
			{
				return Pair.Key.ToString();
			});
			UE_LOG(LogConcert, Error, TEXT("Authority change rejected objects:\n%s"), *Error);
		}
	}

	TSharedRef<SWidget> SSubmissionControls::BuildRevertButton()
	{
		return SNew(SNegativeActionButton)
			.Text(LOCTEXT("RevertChanges.Label", "Revert"))
			.ToolTipText(this, &SSubmissionControls::GetRevertButtonToolTipText)
			.IsEnabled_Lambda([this](){ return GetSubmissionWorkflow().CanRevert(); })
			.OnClicked_Lambda([this]()
			{
				GetSubmissionWorkflow().RevertChanges();
				return FReply::Handled();
			});
	}

	FText SSubmissionControls::GetRevertButtonToolTipText() const
	{
		switch(GetSubmissionWorkflow().GetRevertability())
		{
		case EChangeRevertability::Revertable: return LOCTEXT("RevertChanges.ToolTip.Upload", "Revert all local changes you've made.");
		case EChangeRevertability::NoChanges: return LOCTEXT("RevertChanges.ToolTip.NoChanges", "Nothing to revert.");
		case EChangeRevertability::UploadInProgress: return LOCTEXT("RevertChanges.ToolTip.InTransit", "Disabled because upload is in progress.");
		default: checkNoEntry(); return FText::GetEmpty();
		}
	}
}

#undef LOCTEXT_NAMESPACE