// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/EChangeRevertability.h"
#include "Data/EChangeUploadability.h"

#include "Delegates/Delegate.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

namespace UE::MultiUserClient
{
	class ISubmissionOperation;
	struct FSubmitStreamChangesResponse;
	struct FSubmitAuthorityChangesRequest;
	struct FSubmitAuthorityChangesResponse;
	
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
		virtual TSharedPtr<ISubmissionOperation> SubmitChanges() = 0;

		/** @return Detailed information about whether Submit can be called */
		virtual EChangeUploadability GetUploadability() const = 0;
		bool CanSubmit() const { return GetUploadability() == EChangeUploadability::Ready; }
		
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnStreamRequestCompleted, const FSubmitStreamChangesResponse&);
		/**
		 * Called whenever a submit operation completes the stream change request stage.
		 * @note No stream changes may have been requested. Check the error code.
		 */
		virtual FOnStreamRequestCompleted& OnStreamRequestCompleted() = 0;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAuthorityRequestCompleted, const FSubmitAuthorityChangesRequest&, const FSubmitAuthorityChangesResponse&);
		/**
		 * Called whenever a submit operation completes the authority change request stage.
		 * @note No authority changes may have been requested. Check the error code.
		 */
		virtual FOnAuthorityRequestCompleted& OnAuthorityRequestCompleted() = 0;

		virtual ~ISubmissionWorkflow() = default;
	};

	/** Shared implementation for ISubmissionWorkflow. */
	class FSubmissionWorkflowBase : public ISubmissionWorkflow
	{
	public:

		//~ Begin ISubmissionWorkflow Interface
		virtual FOnStreamRequestCompleted& OnStreamRequestCompleted() override { return StreamRequestCompletedDelegate; }
		virtual FOnAuthorityRequestCompleted& OnAuthorityRequestCompleted() override { return AuthorityRequestCompletedDelegate; }
		//~ End ISubmissionWorkflow Interface

	protected:
		
		FOnStreamRequestCompleted StreamRequestCompletedDelegate;
		FOnAuthorityRequestCompleted AuthorityRequestCompletedDelegate;
	};
}
