// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/EChangeUploadability.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "Delegates/Delegate.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertSyncClient::Replication
{
	struct FChangeStreamRequest;
	struct FAuthorityChangeRequest;
}

namespace UE::MultiUserClient
{
	class ISubmissionOperation;
	struct FSubmitStreamChangesResponse;
	struct FSubmitAuthorityChangesRequest;
	struct FSubmitAuthorityChangesResponse;

	/** At least StreamRequest or AuthorityRequest need to be valid to form a valid request */
	struct FSubmissionParams
	{
		TOptional<ConcertSyncClient::Replication::FChangeStreamRequest> StreamRequest;
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> AuthorityRequest;
		
		bool IsStreamChangeEmpty() const { return !StreamRequest.IsSet() || StreamRequest->IsEmpty(); }
		bool IsAuthorityChangeEmpty() const { return !AuthorityRequest.IsSet() || AuthorityRequest->IsEmpty(); }
		bool IsEmpty() const { return IsStreamChangeEmpty() && IsAuthorityChangeEmpty(); }
	};
	
	/**
	 * Manages the flow of changing stream and authority on the server.
	 * Handles both submitting and reverting.
	 *
	 * The general flow is as follows:
	 *	1. Submit stream changes
	 *	2. Submit authority changes
	 */
	class ISubmissionWorkflow
	{
	public:

		/**
		 * Synchronizes the server with the locally made changes to streams and authority.
		 *
		 * This function creates an operation object which emits special events.
		 * If there is already an operation in progress, this function fails.
		 * @see CanSubmit
		 *
		 * @note The operation might start and instantly stop before SubmitChanges finishes (e.g. a network request fails to be created instantly).
		 * @return The operation object if the operation was started
		 */
		virtual TSharedPtr<ISubmissionOperation> SubmitChanges(FSubmissionParams Params) = 0;

		/** @return Detailed information about whether Submit can be called */
		virtual EChangeUploadability GetUploadability() const = 0;
		bool CanSubmit() const { return GetUploadability() == EChangeUploadability::Ready; }
		
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnStreamRequestCompleted, const FSubmitStreamChangesResponse&);
		/**
		 * Broadcasts whenever a submit operation completes the stream change request stage. Useful for accumulating and counting errors.
		 * @note No stream changes may have been requested. Check the error code.
		 */
		virtual FOnStreamRequestCompleted& OnStreamRequestCompleted() = 0;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAuthorityRequestCompleted, const FSubmitAuthorityChangesRequest&, const FSubmitAuthorityChangesResponse&);
		/**
		 * Broadcasts whenever a submit operation completes the authority change request stage. Useful for accumulating and counting errors.
		 * @note No authority changes may have been requested. Check the error code.
		 */
		virtual FOnAuthorityRequestCompleted& OnAuthorityRequestCompleted() = 0;

		DECLARE_MULTICAST_DELEGATE(FOnSubmitOperationCompleted);
		/** Broadcasts whenever a submit operation completes. Useful for enqueuing submit operations. */
		virtual FOnSubmitOperationCompleted& OnSubmitOperationCompleted() = 0;

		virtual ~ISubmissionWorkflow() = default;
	};

	/** Shared implementation for ISubmissionWorkflow. */
	class FSubmissionWorkflowBase : public ISubmissionWorkflow
	{
	public:

		//~ Begin ISubmissionWorkflow Interface
		virtual FOnStreamRequestCompleted& OnStreamRequestCompleted() override { return StreamRequestCompletedDelegate; }
		virtual FOnAuthorityRequestCompleted& OnAuthorityRequestCompleted() override { return AuthorityRequestCompletedDelegate; }
		virtual FOnSubmitOperationCompleted& OnSubmitOperationCompleted() override { return OnSubmitOperationCompletedDelegate; }
		//~ End ISubmissionWorkflow Interface

	protected:
		
		FOnStreamRequestCompleted StreamRequestCompletedDelegate;
		FOnAuthorityRequestCompleted AuthorityRequestCompletedDelegate;
		FOnSubmitOperationCompleted OnSubmitOperationCompletedDelegate;
	};
}
