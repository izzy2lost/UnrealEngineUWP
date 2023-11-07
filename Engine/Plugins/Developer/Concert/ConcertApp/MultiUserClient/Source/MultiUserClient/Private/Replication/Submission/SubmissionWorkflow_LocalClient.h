// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISubmissionWorkflow.h"

#include "Replication/Stream/IClientStreamSynchronizer.h"
#include "Replication/IToken.h"
#include "SingleClientSubmissionOperation.h"

#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	class IClientAuthoritySynchronizer;
	class IClientStreamSynchronizer;
	class FAuthorityChangeTracker;
	class FGlobalAuthorityCache;
	class FStreamChangeTracker;
	
	/** Handles the submission workflow for a single client. */
	class FSubmissionWorkflow_LocalClient : public FSubmissionWorkflowBase, public FNoncopyable
	{
	public:
		
		FSubmissionWorkflow_LocalClient(
			TSharedRef<IConcertSyncClient> InClient,
			FStreamChangeTracker& InStreamChangeTracker,
			FAuthorityChangeTracker& InAuthorityChangeTracker,
			IClientStreamSynchronizer& InStreamSynchronizer,
			const FGlobalAuthorityCache& InAuthorityCache
			);
		
		//~ Begin ISubmissionWorkflow Interface
		virtual TSharedPtr<ISubmissionOperation> SubmitChanges() override;
		virtual EChangeUploadability GetUploadability() const override;
		virtual EChangeRevertability GetRevertability() const override;
		//~ End ISubmissionWorkflow Interface
	
	private:

		/** Latent operations keep a weak reference to this to detect whether this FSubmissionWorkflow_LocalClient was destroyed. */
		const TSharedRef<FToken> LifetimeToken = FToken::Make();
		
		/** Used to send authority requests to the server. */
		const TSharedRef<IConcertSyncClient> Client;

		/** Used to filter out changes that would cause a conflict when submitted. */
		const FGlobalAuthorityCache& AuthorityCache;

		/** Used to get changes made to the stream */
		FStreamChangeTracker& StreamChangeTracker;
		/** Used to get changes made to the authority */
		FAuthorityChangeTracker& AuthorityChangeTracker;

		/** Used to change the streams */
		IClientStreamSynchronizer& StreamSynchronizer;

		/**
		 * Set for as long as there is a SubmitChanges operation in progress.
		 * Automatically cancels pending promises when destroyed.
		 */
		TOptional<TSharedRef<FSingleClientSubmissionOperation>> InProgressOperation;

		FGuid GetLocalClientStreamId() const { return StreamSynchronizer.GetStreamId(); }

		/** Advances the request by requesting authority. */
		void OnStreamChangeCompleted(
			const ConcertSyncClient::Replication::FChangeStreamRequest& StreamChangeRequest,
			const ConcertSyncClient::Replication::FChangeStreamResponse& ChangeStreamResponse,
			ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityChangeRequest
			);
		void SendAuthorityChangeRequest(ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityChangeRequest);
	};
}

