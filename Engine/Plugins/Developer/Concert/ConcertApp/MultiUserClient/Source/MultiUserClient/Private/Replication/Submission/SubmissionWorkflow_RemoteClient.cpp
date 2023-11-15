// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubmissionWorkflow_RemoteClient.h"

namespace UE::MultiUserClient
{
	TSharedPtr<ISubmissionOperation> FSubmissionWorkflow_RemoteClient::SubmitChanges()
	{
		// TODO DP UE-180657: Changing remote client's stream is not implemented for now
		// TODO DP UE-198088 changing remote client authority
		return nullptr;
	}

	EChangeUploadability FSubmissionWorkflow_RemoteClient::GetUploadability() const
	{
		// TODO DP UE-180657: Changing remote client's stream is not implemented for now
		// TODO DP UE-198088 changing remote client authority
		return EChangeUploadability::NotImplemented;
	}
}
