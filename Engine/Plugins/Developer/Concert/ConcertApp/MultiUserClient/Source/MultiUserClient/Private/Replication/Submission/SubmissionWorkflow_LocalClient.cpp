// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubmissionWorkflow_LocalClient.h"

#include "IConcertSyncClient.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Replication/Util/StreamRequestUtils.h"

namespace UE::MultiUserClient
{
	FSubmissionWorkflow_LocalClient::FSubmissionWorkflow_LocalClient(
		TSharedRef<IConcertSyncClient> InClient,
		FStreamChangeTracker& InStreamChangeTracker,
		FAuthorityChangeTracker& InAuthorityChangeTracker,
		IClientStreamSynchronizer& InStreamSynchronizer,
		const FGlobalAuthorityCache& InAuthorityCache
		)
		: Client(MoveTemp(InClient))
		, AuthorityCache(InAuthorityCache)
		, StreamChangeTracker(InStreamChangeTracker)
		, AuthorityChangeTracker(InAuthorityChangeTracker)
		, StreamSynchronizer(InStreamSynchronizer)
	{}

	EChangeUploadability FSubmissionWorkflow_LocalClient::GetUploadability() const
	{
		const bool bOperationInProgress = InProgressOperation.IsSet(); 
		if (bOperationInProgress)
		{
			return EChangeUploadability::InProgress;
		}

		const bool bHasChanges = StreamChangeTracker.HasChanges() || AuthorityChangeTracker.HasChanges();
		return bHasChanges ? EChangeUploadability::Ready : EChangeUploadability::NoChanges;
	}

	TSharedPtr<ISubmissionOperation> FSubmissionWorkflow_LocalClient::SubmitChanges()
	{
		using namespace ConcertSyncClient::Replication;
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (!CanSubmit() || !ensure(ReplicationManager))
		{
			return nullptr;
		}
		
		// The authority request is pre-built now to avoid sending changes the local client makes while we're waiting for the latent server responses
		FAuthorityChangeRequest AuthorityChangeRequest = AuthorityChangeTracker.BuildChangeRequest(GetLocalClientStreamId());
		
		const FStreamChangelist& Changelist = StreamChangeTracker.GetCachedDeltaChange();
		const bool bIsChangelistEmpty = Changelist.ObjectsToPut.IsEmpty() && Changelist.ObjectsToRemove.IsEmpty();
		const bool bModifyStreams = !bIsChangelistEmpty;
		
		const TSharedRef<FSingleClientSubmissionOperation> Operation = MakeShared<FSingleClientSubmissionOperation>(bModifyStreams);
		InProgressOperation.Emplace(Operation);
		
		if (bIsChangelistEmpty)
		{
			const FSubmitStreamChangesResponse CompletedChange { EStreamSubmissionErrorCode::NoChange };
			Operation->EmplaceStreamPromise(CompletedChange);
			StreamRequestCompletedDelegate.Broadcast(CompletedChange);
			
			SendAuthorityChangeRequest(MoveTemp(AuthorityChangeRequest));
		}
		else
		{
			FChangeStreamRequest StreamRequest = StreamSynchronizer.GetServerState().ReplicatedObjects.IsEmpty()
				? StreamRequestUtils::BuildChangeRequest_CreateNewStream(GetLocalClientStreamId(), Changelist)
				: StreamRequestUtils::BuildChangeRequest_UpdateExistingStream(Changelist);
			
			// Predict conflicts in case the remote client's streams have changed. Also: while the UI highlights "bad" requests, it does not correct it.
			AuthorityCache.CleanseConflictsFromStreamRequest(StreamRequest, GetLocalClientId());
		
			ReplicationManager->ChangeStream(StreamRequest)
				.Next([this, DestructionDetection = LifetimeToken->AsWeak(), StreamRequest, AuthorityChangeRequest = MoveTemp(AuthorityChangeRequest)](FChangeStreamResponse&& Response)
				{
					const FSubmitStreamChangesResponse SubmissionResult { EStreamSubmissionErrorCode::Success, { FCompletedChangeSubmission{StreamRequest, Response } } };
					// The request might execute after we're destroyed, e.g. by leaving session while request is on the way.
					// In that case, the Concert session triggers the OnSessionConnectionChanged which destroys us. Only after that, all the requests are timed out.
					if (DestructionDetection.IsValid())
					{
						OnStreamChangeCompleted(StreamRequest, Response, AuthorityChangeRequest);
					}

					return SubmissionResult;
				});
		}

		// Note that InProgressOperation might already be unset due to ChangeStream failing instantly
		return Operation;
	}

	void FSubmissionWorkflow_LocalClient::OnStreamChangeCompleted(
		const ConcertSyncClient::Replication::FChangeStreamRequest& StreamChangeRequest,
		const ConcertSyncClient::Replication::FChangeStreamResponse& ChangeStreamResponse,
		ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityChangeRequest
		)
	{
		const TSharedRef<FSingleClientSubmissionOperation> Operation = *InProgressOperation;
		
		const EStreamSubmissionErrorCode ErrorCode = ChangeStreamResponse.ErrorCode == EReplicationResponseErrorCode::Handled
			? EStreamSubmissionErrorCode::Success
			: EStreamSubmissionErrorCode::Timeout;
		const FSubmitStreamChangesResponse CompletedChange{ ErrorCode, { FCompletedChangeSubmission{ StreamChangeRequest, ChangeStreamResponse }} };
		Operation->EmplaceStreamPromise(CompletedChange);
		StreamRequestCompletedDelegate.Broadcast(CompletedChange);
		
		if (ChangeStreamResponse.IsSuccess())
		{
			SendAuthorityChangeRequest(MoveTemp(AuthorityChangeRequest));
		}
		else
		{
			const FSubmitAuthorityChangesRequest Request{ EAuthoritySubmissionRequestErrorCode::CancelledDueToStreamUpdate };
			const FSubmitAuthorityChangesResponse Response{ EAuthoritySubmissionResponseErrorCode::CancelledDueToStreamUpdate };
			
			Operation->EmplaceAuthorityRequestPromise(Request);
			Operation->EmplaceAuthorityResponsePromise(Response);
			InProgressOperation.Reset();
			
			AuthorityRequestCompletedDelegate.Broadcast(Request, Response);
		}
	}

	void FSubmissionWorkflow_LocalClient::SendAuthorityChangeRequest(ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityChangeRequest)
	{
		using namespace ConcertSyncClient::Replication;
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (!ensure(ReplicationManager))
		{
			InProgressOperation.Reset(); // Automatically cancels the pending promises
			AuthorityRequestCompletedDelegate.Broadcast({ EAuthoritySubmissionRequestErrorCode::Cancelled }, { EAuthoritySubmissionResponseErrorCode::Cancelled });
			return;
		}

		const TSharedRef<FSingleClientSubmissionOperation> Operation = *InProgressOperation;
		const bool bHasNoChanges = AuthorityChangeRequest.ReleaseAuthority.IsEmpty() && AuthorityChangeRequest.TakeAuthority.IsEmpty();
		if (bHasNoChanges)
		{
			const FSubmitAuthorityChangesRequest Request{ EAuthoritySubmissionRequestErrorCode::NoChange };
			const FSubmitAuthorityChangesResponse Response{ EAuthoritySubmissionResponseErrorCode::NoChange };
			
			Operation->EmplaceAuthorityRequestPromise(Request);
			Operation->EmplaceAuthorityResponsePromise(Response);
			InProgressOperation.Reset();
			
			AuthorityRequestCompletedDelegate.Broadcast(Request, Response);
			return;
		}

		// Predict conflicts in case the remote client's streams have changed. Also: while the UI highlights "bad" requests, it does not correct it.
		AuthorityCache.CleanseConflictsFromAuthorityRequest(AuthorityChangeRequest, GetLocalClientId());
		
		FSubmitAuthorityChangesRequest Request{ EAuthoritySubmissionRequestErrorCode::Success, AuthorityChangeRequest };
		Operation->EmplaceAuthorityRequestPromise(Request);
		ReplicationManager->RequestAuthorityChange(MoveTemp(AuthorityChangeRequest))
			.Next([this, Request = MoveTemp(Request), DestructionDetection = LifetimeToken->AsWeak()](FAuthorityChangeResponse&& Response)
			{
				if (DestructionDetection.IsValid())
				{
					const EAuthoritySubmissionResponseErrorCode ErrorCode = Response.ErrorCode == EReplicationResponseErrorCode::Handled
						? EAuthoritySubmissionResponseErrorCode::Success
						: EAuthoritySubmissionResponseErrorCode::Timeout;
					const FSubmitAuthorityChangesResponse Result { ErrorCode, MoveTemp(Response) };
					
					InProgressOperation->Get().EmplaceAuthorityResponsePromise(Result);
					InProgressOperation.Reset();
					
					AuthorityRequestCompletedDelegate.Broadcast(Request, Result);
				}
			});
	}
	
	FGuid FSubmissionWorkflow_LocalClient::GetLocalClientId() const
	{
		return Client->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId();
	}
}
