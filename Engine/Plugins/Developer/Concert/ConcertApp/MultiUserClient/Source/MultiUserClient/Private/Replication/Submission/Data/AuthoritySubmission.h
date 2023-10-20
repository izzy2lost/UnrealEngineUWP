// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"

namespace UE::MultiUserClient
{
	class ISubmissionOperation;
	
	enum class EAuthoritySubmissionErrorCode
	{
		/** The operation completed as expected */
		Success,

		/** No change was sent to the server because there were no local changes. */
		NoChange, 

		/** The request timed out */
		Timeout,

		/** Cancelled because the required stream update could not be made. */
		CancelledDueToStreamUpdate,
		
		/** The request was cancelled (e.g. because client disconnected while operation was in progress) */
		Cancelled
	};

	FText LexToText(EAuthoritySubmissionErrorCode ErrorCode);

	struct FSubmitAuthorityChangesRequest
	{
		EAuthoritySubmissionErrorCode ErrorCode;
		
		/** The operation this was executed as part of. You cannot keep any reference to this. */
		ISubmissionOperation& OperationContext;
		
		/** Only valid if ErrorCode == EAuthoritySubmissionErrorCode::Success */
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> Request;
	};
	
	struct FSubmitAuthorityChangesResponse
	{
		EAuthoritySubmissionErrorCode ErrorCode;
		
		/** The operation this was executed as part of. You cannot keep any reference to this. */
		ISubmissionOperation& OperationContext;
		
		/** Only valid if ErrorCode == EAuthoritySubmissionErrorCode::Success */
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeResponse> Response;
	};
}
