// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Submission/ISubmissionOperation.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SNotificationItem;

namespace UE::MultiUserClient
{
	class ISubmissionWorkflow;
	/**
	 * Displays a ISubmissionWorkflow.
	 * 
	 * Contains an upload and revert button.
	 * Displays notifications to the user.
	 */
	class SSubmissionControls : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SSubmissionControls)
		{}
			SLATE_ATTRIBUTE(ISubmissionWorkflow*, SubmissionWorkflow)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		/** Used to get the submission workflow that is being displayed. */
		TAttribute<ISubmissionWorkflow*> SubmissionWorkflowAttribute;
		
		ISubmissionWorkflow& GetSubmissionWorkflow() const
		{
			ISubmissionWorkflow* Workflow = SubmissionWorkflowAttribute.Get();
			check(Workflow);
			return *Workflow;
		}

		// Upload button
		TSharedRef<SWidget> BuildUploadButton();
		FText GetUploadButtonToolTipText() const;
		FReply OnUploadButtonClicked() const;

		// Handle submission events
		void SetupNotificationsFor(ISubmissionOperation& SubmissionOperation) const;
		void HandleStreamUpdatedNotification(ISubmissionOperation& SubmissionOperation) const;
		void HandleAuthorityChangeRequestedNotification(ISubmissionOperation& SubmissionOperation) const;
		static void HandleAuthorityChangedResponseNotification(
			const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request,
			const FSubmitAuthorityChangesResponse& ResponseMetaData,
			const TSharedRef<SNotificationItem>& NotificationItem
			);

		// Revert button
		TSharedRef<SWidget> BuildRevertButton();
		FText GetRevertButtonToolTipText() const;
	};
}


