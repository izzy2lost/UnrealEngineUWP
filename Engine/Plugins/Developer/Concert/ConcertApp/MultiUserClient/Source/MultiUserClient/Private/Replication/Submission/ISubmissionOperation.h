// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Data/AuthoritySubmission.h"
#include "Data/StreamSubmission.h"

namespace UE::MultiUserClient
{
	enum class ESubmissionOperationCompletedCode
	{
		/** The operation was processed normally */
		Processed,
		/** Cancelled, e.g. because the workflow was destroyed */
		Cancelled
	};
	
	/**
	 * Exposes the stages of submitting to the server.
	 * Every time ISubmissionWorkflow::SubmitChanges is called a new instance is created.
	 */
	class ISubmissionOperation
	{
	public:

		/** @return Whether a request for changing streams has been or will be sent. */
		virtual bool IsModifyingStreams() const = 0;
		
		/**
		 * Completes when the operation of changing streams has completed.
		 * @note This can be called at most once; subsequent calls result in an unset future.
		 */
		virtual TFuture<FSubmitStreamChangesResponse> OnCompleteStreamChangesFuture() = 0;

		/**
		 * Completes when the authority change request has been sent to the server.
		 * @note This can be called at most once; subsequent calls result in an unset future.
		 */
		virtual TFuture<FSubmitAuthorityChangesRequest> OnRequestAuthorityChangeFuture() = 0;
		
		/**
		 * Completes when the operation of changing authority has completed.
		 * @note This can be called at most once; subsequent calls result in an unset future.
		 */
		virtual TFuture<FSubmitAuthorityChangesResponse> OnCompleteAuthorityChangeFuture() = 0;

		/**
		 * Completes when the operation is done. No further work will be performed.
		 * @note This can be called at most once; subsequent calls result in an unset future.
		 */
		virtual TFuture<ESubmissionOperationCompletedCode> OnCompletedOperation() = 0;
		
		virtual ~ISubmissionOperation() = default;
	};
}
