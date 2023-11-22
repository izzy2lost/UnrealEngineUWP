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
		
		FSubmissionWorkflow_LocalClient(TSharedRef<IConcertSyncClient> InClient);
		
		//~ Begin ISubmissionWorkflow Interface
		virtual TSharedPtr<ISubmissionOperation> SubmitChanges(FSubmissionParams Params) override;
		virtual EChangeUploadability GetUploadability() const override;
		//~ End ISubmissionWorkflow Interface
	
	private:

		/** Latent operations keep a weak reference to this to detect whether this FSubmissionWorkflow_LocalClient was destroyed. */
		const TSharedRef<FToken> LifetimeToken = FToken::Make();
		
		/** Used to send authority requests to the server. */
		const TSharedRef<IConcertSyncClient> Client;

		struct FOperationData
		{
			TSharedRef<FSingleClientSubmissionOperation> Operation;

			DECLARE_DELEGATE(FOnDestroy);
			FOnDestroy OnDestroy;
			
			~FOperationData()
			{
				OnDestroy.ExecuteIfBound();
			}
		};
		
		/**
		 * Set for as long as there is a SubmitChanges operation in progress.
		 * Automatically cancels pending promises when destroyed.
		 */
		TOptional<FOperationData> InProgressOperation;

		/** Advances the request by requesting authority. */
		void OnStreamChangeCompleted(
			const ConcertSyncClient::Replication::FChangeStreamRequest& StreamChangeRequest,
			const ConcertSyncClient::Replication::FChangeStreamResponse& ChangeStreamResponse,
			TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> AuthorityChangeRequest
			);
		void HandlePendingAuthorityChangeRequest(TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> AuthorityChangeRequest);
	};
}

