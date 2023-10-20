// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISubmissionWorkflow.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	class FSubmissionWorkflow_RemoteClient
		: public ISubmissionWorkflow
		, public FNoncopyable
	{
	public:

		//~ Begin ISubmissionWorkflow Interface
		virtual ISubmissionOperation* SubmitChanges() override;
		virtual void RevertChanges() override;
		virtual EChangeUploadability GetUploadability() const override;
		virtual EChangeRevertability GetRevertability() const override;
		//~ End ISubmissionWorkflow Interface
	};
}

