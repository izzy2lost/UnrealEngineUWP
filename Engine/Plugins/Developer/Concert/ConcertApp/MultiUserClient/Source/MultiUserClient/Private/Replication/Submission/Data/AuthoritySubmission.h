// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"

namespace UE::MultiUserClient
{
	class ISubmissionOperation;
	
	enum class EAuthoritySubmissionRequestErrorCode
	{
		/** The operation completed as expected */
		Success,

		/** No change was sent to the server because there were no local changes. */
		NoChange, 

		/** Cancelled because the required stream update could not be made. */
		CancelledDueToStreamUpdate,
		
		/** The request was cancelled (e.g. because client disconnected while operation was in progress) */
		Cancelled
	};

	enum class EAuthoritySubmissionResponseErrorCode
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

	FText LexToText(EAuthoritySubmissionResponseErrorCode ErrorCode);

	struct FSubmitAuthorityChangesRequest
	{
		EAuthoritySubmissionRequestErrorCode ErrorCode;
		
		/** Only valid if ErrorCode == EAuthoritySubmissionErrorCode::Success */
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeRequest> Request;
	};
	
	struct FSubmitAuthorityChangesResponse
	{
		EAuthoritySubmissionResponseErrorCode ErrorCode = EAuthoritySubmissionResponseErrorCode::Cancelled;
		
		/** Only valid if ErrorCode == EAuthoritySubmissionErrorCode::Success */
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeResponse> Response;
	};
}
