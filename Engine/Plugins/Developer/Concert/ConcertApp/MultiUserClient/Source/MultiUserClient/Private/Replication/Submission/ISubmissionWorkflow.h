// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/EChangeRevertability.h"
#include "Data/EChangeUploadability.h"

#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

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
