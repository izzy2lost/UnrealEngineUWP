// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubmissionWorkflow_LocalClient.h"

#include "IConcertSyncClient.h"
#include "Replication/IConcertClientReplicationManager.h"

namespace UE::MultiUserClient
{
	FSubmissionWorkflow_LocalClient::FSubmissionWorkflow_LocalClient(TSharedRef<IConcertSyncClient> InClient)
		: Client(MoveTemp(InClient))
	{}

	EChangeUploadability FSubmissionWorkflow_LocalClient::GetUploadability() const
	{
		const bool bOperationInProgress = InProgressOperation.IsSet(); 
		return bOperationInProgress ? EChangeUploadability::InProgress : EChangeUploadability::Ready;
	}

	TSharedPtr<ISubmissionOperation> FSubmissionWorkflow_LocalClient::SubmitChanges(FSubmissionParams Params)
	{
		using namespace ConcertSyncClient::Replication;
		
		const TOptional<FChangeStreamRequest>& OptionalStreamRequest = Params.StreamRequest;
		TOptional<FAuthorityChangeRequest>& OptionalAuthorityRequest = Params.AuthorityRequest;
		
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (Params.IsEmpty() || !CanSubmit() || !ensure(ReplicationManager))
		{
			return nullptr;
		}
		
		const bool bIsStreamChangeEmpty = Params.IsStreamChangeEmpty();
		const bool bModifyStreams = !bIsStreamChangeEmpty;
		const TSharedRef<FSingleClientSubmissionOperation> Operation = MakeShared<FSingleClientSubmissionOperation>(bModifyStreams);
		InProgressOperation = { Operation };
		InProgressOperation->OnDestroy.BindLambda([this](){ OnSubmitOperationCompletedDelegate.Broadcast(); });
		
		if (bIsStreamChangeEmpty)
		{
			const FSubmitStreamChangesResponse CompletedChange { EStreamSubmissionErrorCode::NoChange };
			Operation->EmplaceStreamPromise(CompletedChange);
			StreamRequestCompletedDelegate.Broadcast(CompletedChange);
			
			HandlePendingAuthorityChangeRequest(MoveTemp(*OptionalAuthorityRequest));
		}
		else
		{
			ReplicationManager->ChangeStream(*OptionalStreamRequest)
				.Next([this, DestructionDetection = LifetimeToken->AsWeak(), OptionalStreamRequest, AuthorityRequest = MoveTemp(OptionalAuthorityRequest)](FChangeStreamResponse&& Response) mutable
				{
					const FSubmitStreamChangesResponse SubmissionResult { EStreamSubmissionErrorCode::Success, { FCompletedChangeSubmission{*OptionalStreamRequest, Response } } };
					// The request might execute after we're destroyed, e.g. by leaving session while request is on the way.
					// In that case, the Concert session triggers the OnSessionConnectionChanged which destroys us. Only after that, all the requests are timed out.
					if (DestructionDetection.IsValid())
					{
						OnStreamChangeCompleted(*OptionalStreamRequest, Response, MoveTemp(AuthorityRequest));
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
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> AuthorityChangeRequest
		)
	{
		const TSharedRef<FSingleClientSubmissionOperation> Operation = InProgressOperation->Operation;
		
		const EStreamSubmissionErrorCode ErrorCode = ChangeStreamResponse.ErrorCode == EReplicationResponseErrorCode::Handled
			? EStreamSubmissionErrorCode::Success
			: EStreamSubmissionErrorCode::Timeout;
		const FSubmitStreamChangesResponse CompletedChange{ ErrorCode, { FCompletedChangeSubmission{ StreamChangeRequest, ChangeStreamResponse }} };
		Operation->EmplaceStreamPromise(CompletedChange);
		StreamRequestCompletedDelegate.Broadcast(CompletedChange);
		
		if (ChangeStreamResponse.IsSuccess())
		{
			HandlePendingAuthorityChangeRequest(MoveTemp(AuthorityChangeRequest));
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

	void FSubmissionWorkflow_LocalClient::HandlePendingAuthorityChangeRequest(TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> AuthorityChangeRequest)
	{
		using namespace ConcertSyncClient::Replication;
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (!ensure(ReplicationManager))
		{
			InProgressOperation.Reset(); // Automatically cancels the pending promises
			AuthorityRequestCompletedDelegate.Broadcast({ EAuthoritySubmissionRequestErrorCode::Cancelled }, { EAuthoritySubmissionResponseErrorCode::Cancelled });
			return;
		}

		const TSharedRef<FSingleClientSubmissionOperation> Operation = InProgressOperation->Operation;
		const bool bHasNoChanges = !AuthorityChangeRequest || AuthorityChangeRequest->IsEmpty();
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
		
		FSubmitAuthorityChangesRequest Request{ EAuthoritySubmissionRequestErrorCode::Success, AuthorityChangeRequest };
		Operation->EmplaceAuthorityRequestPromise(Request);
		ReplicationManager->RequestAuthorityChange(MoveTemp(*AuthorityChangeRequest))
			.Next([this, Request = MoveTemp(Request), DestructionDetection = LifetimeToken->AsWeak()](FAuthorityChangeResponse&& Response)
			{
				if (DestructionDetection.IsValid())
				{
					const EAuthoritySubmissionResponseErrorCode ErrorCode = Response.ErrorCode == EReplicationResponseErrorCode::Handled
						? EAuthoritySubmissionResponseErrorCode::Success
						: EAuthoritySubmissionResponseErrorCode::Timeout;
					const FSubmitAuthorityChangesResponse Result { ErrorCode, MoveTemp(Response) };
					
					InProgressOperation->Operation->EmplaceAuthorityResponsePromise(Result);
					InProgressOperation.Reset();
					
					AuthorityRequestCompletedDelegate.Broadcast(Request, Result);
				}
			});
	}
}
