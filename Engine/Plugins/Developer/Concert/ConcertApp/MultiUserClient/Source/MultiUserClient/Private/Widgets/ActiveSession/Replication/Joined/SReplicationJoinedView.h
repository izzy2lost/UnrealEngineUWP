// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SNotificationItem;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationEditorView;
}

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;

	/** This widget is displayed by SReplicationRootWidget when the client has joined replication. */
	class SReplicationJoinedView : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationJoinedView)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager);

	private:

		/** Acts as the model of this view */
		TSharedPtr<FMultiUserReplicationManager> ReplicationManager;
		
		/**
		 * The editor view if one is being displayed.
		 * ChildSlot keeps the reference alive if we're in state EMultiUserReplicationConnectionState::Connected.
		 * @see ShowWidget_Connected
		 */
		TWeakPtr<ConcertClientSharedSlate::IReplicationEditorView> WeakEditorView;

		/** Notification about in progress authority change, if any. */
		TSharedPtr<SNotificationItem> AuthorityChangeNotification;
		
		/** Called when any of the streams change. */
		void OnModelChanged() const;

		void OnAuthorityRequestSent_AnyThread(const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request);
		void OnAuthorityResponseReceived_AnyThread(
			const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request,
			const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response
			);
	};
}
