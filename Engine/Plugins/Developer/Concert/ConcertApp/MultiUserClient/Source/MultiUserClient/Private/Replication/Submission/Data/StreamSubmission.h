// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"

namespace UE::MultiUserClient
{
	class ISubmissionOperation;

	enum class EStreamSubmissionErrorCode
	{
		/** Changes were submitted to and processed by the server */
		Success,

		/** No change was sent to the server because there were no local changes. */
		NoChange,

		/** The request was cancelled (e.g. because client disconnected while operation was in progress) */
		Cancelled
	};

	/** Contains the original request and response of a completed stream change. */
	struct FCompletedChangeSubmission
	{
		ConcertSyncClient::Replication::FChangeStreamRequest Request;
		ConcertSyncClient::Replication::FChangeStreamResponse Response;
	};

	/** Resulting of submitting stream changes */
	struct FSubmitStreamChangesResponse
	{
		/** Error code of the submission. Determines whether Response is valid. */
		EStreamSubmissionErrorCode ErrorCode;

		/** The operation this was executed as part of. You cannot keep any reference to this. */
		ISubmissionOperation& OperationContext; 

		/** Valid if ErrorCode != ESubmitChangesErrorCode::AlreadyInProgress. */
		TOptional<FCompletedChangeSubmission> SubmissionInfo;
	};
}
