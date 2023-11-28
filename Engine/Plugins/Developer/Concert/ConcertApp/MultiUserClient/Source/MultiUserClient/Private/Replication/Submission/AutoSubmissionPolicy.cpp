// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSubmissionPolicy.h"

#include "ChangeRequestBuilder.h"
#include "ISubmissionWorkflow.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Submission/Queue/SubmissionQueue.h"

namespace UE::MultiUserClient
{
	FAutoSubmissionPolicy::FAutoSubmissionPolicy(
		FSubmissionQueue& InSubmissionQueue,
		const FChangeRequestBuilder& InRequestBuilder,
		ConcertClientSharedSlate::IEditableReplicationStreamModel& InStreamEditorModel,
		FAuthorityChangeTracker& InAuthorityChangeTracker
		)
		: FSelfUnregisteringDeferredSubmitter(InSubmissionQueue)
		, SubmissionQueue(InSubmissionQueue)
		, RequestBuilder(InRequestBuilder)
		, StreamEditorModel(InStreamEditorModel)
		, AuthorityChangeTracker(InAuthorityChangeTracker)
	{
		StreamEditorModel.OnObjectsChanged().AddRaw(this, &FAutoSubmissionPolicy::OnObjectsChanged);
		StreamEditorModel.OnPropertiesChanged().AddRaw(this, &FAutoSubmissionPolicy::OnChangesDetected);
		AuthorityChangeTracker.OnAddedOwnedObjects().AddRaw(this, &FAutoSubmissionPolicy::OnChangesDetected);
	}

	FAutoSubmissionPolicy::~FAutoSubmissionPolicy()
	{
		StreamEditorModel.OnObjectsChanged().RemoveAll(this);
		StreamEditorModel.OnPropertiesChanged().RemoveAll(this);
		AuthorityChangeTracker.OnAddedOwnedObjects().RemoveAll(this);
	}

	void FAutoSubmissionPolicy::ProcessAccumulatedChangesAndSubmit()
	{
		if (bIsDirty)
		{
			SubmissionQueue.SubmitNowOrEnqueue_GameThread(*this);
		}
	}

	void FAutoSubmissionPolicy::PerformSubmission_GameThread(ISubmissionWorkflow& Workflow)
	{
		using namespace UE::ConcertSyncClient::Replication;
		bIsDirty = false;
			
		// Even though authority request is sent after server confirms stream change, the authority request is pre-built to avoid sending changes
		// the local client makes while we're waiting for the latent server responses.
		TOptional<FAuthorityChangeRequest> AuthorityChangeRequest = RequestBuilder.BuildAuthorityChange();
		TOptional<FChangeStreamRequest> StreamRequest = RequestBuilder.BuildStreamChange();
			
		if (AuthorityChangeRequest || StreamRequest)
		{
			Workflow.SubmitChanges({ StreamRequest, AuthorityChangeRequest });
		}
	}

	void FAutoSubmissionPolicy::OnChangesDetected()
	{
		bIsDirty = true;
	}
}
