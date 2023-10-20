// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Submission/Data/AuthoritySubmission.h"

#define LOCTEXT_NAMESPACE "EAuthoritySubmissionErrorCode"

namespace UE::MultiUserClient
{
	FText LexToText(EAuthoritySubmissionErrorCode ErrorCode)
	{
		switch (ErrorCode)
		{
		case EAuthoritySubmissionErrorCode::Success: return LOCTEXT("Success", "Success");
		case EAuthoritySubmissionErrorCode::NoChange: return LOCTEXT("NoChange", "No Change");
		case EAuthoritySubmissionErrorCode::Timeout: return LOCTEXT("Timeout", "Timeout");
		case EAuthoritySubmissionErrorCode::CancelledDueToStreamUpdate: return LOCTEXT("CancelledDueToStreamUpdate", "Failed because stream dependency could not be updated");
		case EAuthoritySubmissionErrorCode::Cancelled: return LOCTEXT("Cancelled", "Cancelled");
		default:
			checkNoEntry();
			return LOCTEXT("Unknown", "Unknown");;
		}
	}
}

#undef LOCTEXT_NAMESPACE