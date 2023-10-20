// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/EChangeRevertability.h"
#include "Data/EChangeUploadability.h"

namespace UE::MultiUserClient
{
	class ISubmissionOperation;
	struct FSubmitStreamChangesResponse;
	
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
		 * Synchronizes the server with the locally made changes streams and authority.
		 * @return Operation object which emits special events. You cannot keep any reference to it. Null if there is already an operation in progress.
		 * @see CanSubmit
		 */
		virtual ISubmissionOperation* SubmitChanges() = 0;

		/** Reverts all local changes */
		virtual void RevertChanges() = 0;

		/** @return Detailed information about whether Submit can be called */
		virtual EChangeUploadability GetUploadability() const = 0;
		bool CanSubmit() const { return GetUploadability() == EChangeUploadability::Ready; }

		/** @return Detailed information about whether Revert can be called */
		virtual EChangeRevertability GetRevertability() const = 0;
		bool CanRevert() const { return GetRevertability() == EChangeRevertability::Revertable; }

		virtual ~ISubmissionWorkflow() = default;
	};
}
