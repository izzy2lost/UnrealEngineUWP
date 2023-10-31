// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AccumulatedSubmissionErrors.h"
#include "Templates/UnrealTemplate.h"

class SNotificationItem;
class FReply;

namespace UE::MultiUserClient
{
	class FRemoteReplicationClient;
	class FReplicationClient;
	class FReplicationClientManager;
	
	class SAuthorityRejectedNotification;
	class SStreamRejectedNotification;
	
	struct FSubmitAuthorityChangesResponse;
	struct FSubmitAuthorityChangesRequest;
	struct FSubmitStreamChangesResponse;
	
	/**
	 * Adds a SNotificationItem when ISubmissionWorkflow operations fail.
	 * Failing operations are bundled into a single notification item until it is manually dismissed.
	 */
	class FSubmissionNotifier : public FNoncopyable
	{
	public:
		
		FSubmissionNotifier(FReplicationClientManager& InReplicationClientManager);
		~FSubmissionNotifier();

	private:

		/** Emits events when remote clients are discovered. */
		FReplicationClientManager& ReplicationClientManager;

		/** Only valid if changing streams has failed and the user has not yet dismissed the notification. */
		TSharedPtr<SNotificationItem> StreamNotificationItem;
		/** Only valid if changing authority has failed and the user has not yet dismissed the notification. */
		TSharedPtr<SNotificationItem> AuthorityNotificationItem;
		
		/** Wrapped by StreamNotification */
		TSharedPtr<SStreamRejectedNotification> StreamRejectedNotification;
		/** Wrapped by AuthorityNotification */
		TSharedPtr<SAuthorityRejectedNotification> AuthorityRejectedNotification;

		/** Displayed by StreamRejectedNotification */
		FAccumulatedStreamErrors StreamErrors;
		/** Displayed by AuthorityRejectedNotification */
		FAccumulatedAuthorityErrors AuthorityErrors;
		
		// Client events
		void OnPostRemoteClientAdded(FRemoteReplicationClient& RemoteReplicationClient);
		void RegisterClient(FReplicationClient& Client);
		void UnregisterClient(FReplicationClient& Client);

		// Per-client submission events
		void OnStreamRequestCompleted(const FSubmitStreamChangesResponse& Request);
		void OnAuthorityRequestCompleted(const FSubmitAuthorityChangesRequest& Request, const FSubmitAuthorityChangesResponse& Response);

		// Handle rejections
		void AccumulateStreamRejections(const FSubmitStreamChangesResponse& CompletedOp);
		void AccumulateAuthorityRejections(const FSubmitAuthorityChangesRequest& RequestOp, const FSubmitAuthorityChangesResponse& ResponseOp);
		bool HasStreamRejections() const { return StreamErrors.NumTimeouts > 0 || !StreamErrors.AuthorityConflicts.IsEmpty() || !StreamErrors.SemanticErrors.IsEmpty() || StreamErrors.bFailedStreamCreation; }
		bool HasAuthorityRejections() const { return AuthorityErrors.NumTimeouts > 0 || !AuthorityErrors.Rejected.IsEmpty(); }

		// Updating the notifications
		void CreateOrUpdateStreamNotification();
		void CreateOrUpdateAuthorityNotification();

		// Button events
		FReply CloseStreamNotification();
		FReply CloseAuthorityNotification();
	};
}

