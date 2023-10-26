// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubmissionWorkflow_LocalClient.h"

#include "IConcertSyncClient.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Util/StreamRequestUtils.h"

namespace UE::MultiUserClient
{
	FSubmissionWorkflow_LocalClient::FSubmissionWorkflow_LocalClient(
		TSharedRef<IConcertSyncClient> InClient,
		FStreamChangeTracker& InStreamChangeTracker,
		FAuthorityChangeTracker& InAuthorityChangeTracker,
		IClientStreamSynchronizer& InStreamSynchronizer
		)
		: Client(MoveTemp(InClient))
		, StreamChangeTracker(InStreamChangeTracker)
		, AuthorityChangeTracker(InAuthorityChangeTracker)
		, StreamSynchronizer(InStreamSynchronizer)
	{}

	void FSubmissionWorkflow_LocalClient::RevertChanges()
	{
		StreamChangeTracker.RevertCachedChanges();
		AuthorityChangeTracker.ClearChanges();
	}

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

	EChangeRevertability FSubmissionWorkflow_LocalClient::GetRevertability() const
	{
		if (GetUploadability() == EChangeUploadability::InProgress)
		{
			return EChangeRevertability::UploadInProgress;
		}

		const bool bStreamHasChanges = StreamChangeTracker.HasChanges();
		const bool bAuthorityHasChanges = AuthorityChangeTracker.HasChanges();
		const bool bHasChanges = bStreamHasChanges || bAuthorityHasChanges;
		return bHasChanges ? EChangeRevertability::Revertable : EChangeRevertability::NoChanges;
	}

	TSharedPtr<ISubmissionOperation> FSubmissionWorkflow_LocalClient::SubmitChanges()
	{
		using namespace ConcertSyncClient::Replication;
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (!CanSubmit() || !ensure(ReplicationManager))
		{
			return nullptr;
		}

		// TODO DP: We should check the authority change for conflicts one more time here, in case the remote client's streams have changed since the user last edited authority
		// The authority request is pre-built now to avoid sending changes the local client makes while we're waiting for the latent server responses
		FAuthorityChangeRequest AuthorityChangeRequest = AuthorityChangeTracker.BuildChangeRequest(GetLocalClientStreamId());
		
		const FStreamChangelist& Changelist = StreamChangeTracker.GetCachedDeltaChange();
		const bool bIsChangelistEmpty = Changelist.ObjectsToPut.IsEmpty() && Changelist.ObjectsToRemove.IsEmpty();
		const bool bModifyStreams = !bIsChangelistEmpty;
		
		const TSharedRef<FSingleClientSubmissionOperation> Operation = MakeShared<FSingleClientSubmissionOperation>(bModifyStreams);
		InProgressOperation.Emplace(Operation);
		
		if (bIsChangelistEmpty)
		{
			Operation->EmplaceStreamPromise(FSubmitStreamChangesResponse{ EStreamSubmissionErrorCode::NoChange });
			SendAuthorityChangeRequest(MoveTemp(AuthorityChangeRequest));
		}
		else
		{
			FChangeStreamRequest Request = StreamSynchronizer.GetServerState().ReplicatedObjects.IsEmpty()
				? StreamRequestUtils::BuildChangeRequest_CreateNewStream(GetLocalClientStreamId(), Changelist)
				: StreamRequestUtils::BuildChangeRequest_UpdateExistingStream(Changelist);
		
			ReplicationManager->ChangeStream(Request)
				.Next([this, DestructionDetection = LifetimeToken->AsWeak(), Request, AuthorityChangeRequest = MoveTemp(AuthorityChangeRequest)](FChangeStreamResponse&& Response)
				{
					const FSubmitStreamChangesResponse SubmissionResult { EStreamSubmissionErrorCode::Success, { FCompletedChangeSubmission{Request, Response } } };
					// The request might execute after we're destroyed, e.g. by leaving session while request is on the way.
					// In that case, the Concert session triggers the OnSessionConnectionChanged which destroys us. Only after that, all the requests are timed out.
					if (DestructionDetection.IsValid())
					{
						OnStreamChangeCompleted(Request, Response, AuthorityChangeRequest);
					}

					return SubmissionResult;
				});
		}

		// Note that InProgressOperation might already be unset due to ChangeStream failing instantly
		return Operation;
	}

	void FSubmissionWorkflow_LocalClient::OnStreamChangeCompleted(
		const ConcertSyncClient::Replication::FChangeStreamRequest& Request,
		const ConcertSyncClient::Replication::FChangeStreamResponse& ChangeStreamResponse,
		ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityChangeRequest
		)
	{
		const TSharedRef<FSingleClientSubmissionOperation> Operation = *InProgressOperation;
		
		const EStreamSubmissionErrorCode ErrorCode = ChangeStreamResponse.ErrorCode == EReplicationResponseErrorCode::Handled
			? EStreamSubmissionErrorCode::Success
			: EStreamSubmissionErrorCode::Timeout;
		Operation->EmplaceStreamPromise(FSubmitStreamChangesResponse{
			ErrorCode,
			{ FCompletedChangeSubmission{ Request, ChangeStreamResponse } }
		});
		
		if (ChangeStreamResponse.IsSuccess())
		{
			SendAuthorityChangeRequest(MoveTemp(AuthorityChangeRequest));
		}
		else
		{
			Operation->EmplaceAuthorityRequestPromise(FSubmitAuthorityChangesRequest{ EAuthoritySubmissionRequestErrorCode::CancelledDueToStreamUpdate });
			Operation->EmplaceAuthorityResponsePromise(FSubmitAuthorityChangesResponse{ EAuthoritySubmissionResponseErrorCode::CancelledDueToStreamUpdate });
			InProgressOperation.Reset();
		}
	}

	void FSubmissionWorkflow_LocalClient::SendAuthorityChangeRequest(ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityChangeRequest)
	{
		using namespace ConcertSyncClient::Replication;
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (!ensure(ReplicationManager))
		{
			InProgressOperation.Reset(); // Automatically cancels the pending promises
			return;
		}

		const TSharedRef<FSingleClientSubmissionOperation> Operation = *InProgressOperation;
		const bool bHasNoChanges = AuthorityChangeRequest.ReleaseAuthority.IsEmpty() && AuthorityChangeRequest.TakeAuthority.IsEmpty();
		if (bHasNoChanges)
		{
			Operation->EmplaceAuthorityRequestPromise(FSubmitAuthorityChangesRequest{ EAuthoritySubmissionRequestErrorCode::NoChange });
			Operation->EmplaceAuthorityResponsePromise(FSubmitAuthorityChangesResponse{ EAuthoritySubmissionResponseErrorCode::NoChange });
			InProgressOperation.Reset();
			return;
		}
		
		Operation->EmplaceAuthorityRequestPromise(FSubmitAuthorityChangesRequest{ EAuthoritySubmissionRequestErrorCode::Success, AuthorityChangeRequest });
		ReplicationManager->RequestAuthorityChange(MoveTemp(AuthorityChangeRequest))
			.Next([this, DestructionDetection = LifetimeToken->AsWeak()](FAuthorityChangeResponse&& Response)
			{
				if (DestructionDetection.IsValid())
				{
					const EAuthoritySubmissionResponseErrorCode ErrorCode = Response.ErrorCode == EReplicationResponseErrorCode::Handled
						? EAuthoritySubmissionResponseErrorCode::Success
						: EAuthoritySubmissionResponseErrorCode::Timeout;
					InProgressOperation->Get().EmplaceAuthorityResponsePromise(FSubmitAuthorityChangesResponse{ ErrorCode, MoveTemp(Response) });
					InProgressOperation.Reset();
				}
			});
	}
}
