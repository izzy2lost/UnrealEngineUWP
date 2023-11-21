// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISubmissionWorkflow.h"

#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	class FSubmissionWorkflow_RemoteClient
		: public FSubmissionWorkflowBase
		, public FNoncopyable
	{
	public:

		//~ Begin ISubmissionWorkflow Interface
		virtual TSharedPtr<ISubmissionOperation> SubmitChanges(FSubmissionParams Params) override;
		virtual EChangeUploadability GetUploadability() const override;
		//~ End ISubmissionWorkflow Interface
	};
}

