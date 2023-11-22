// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSubmissionPolicy.h"

#include "ChangeRequestBuilder.h"
#include "ISubmissionWorkflow.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"

#include "Misc/CoreDelegates.h"

namespace UE::MultiUserClient
{
	FAutoSubmissionPolicy::FAutoSubmissionPolicy(
		ISubmissionWorkflow& InSubmissionWorkflow,
		const FChangeRequestBuilder& InRequestBuilder,
		ConcertClientSharedSlate::IEditableReplicationStreamModel& InStreamEditorModel,
		FAuthorityChangeTracker& InAuthorityChangeTracker
		)
		: SubmissionWorkflow(InSubmissionWorkflow)
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
		FCoreDelegates::OnEndFrame.RemoveAll(this);
		
		StreamEditorModel.OnObjectsChanged().RemoveAll(this);
		StreamEditorModel.OnPropertiesChanged().RemoveAll(this);
		AuthorityChangeTracker.OnAddedOwnedObjects().RemoveAll(this);
		
		SubmissionWorkflow.OnStreamRequestCompleted().RemoveAll(this);
	}

	void FAutoSubmissionPolicy::ProcessAccumulatedChangesAndSubmit()
	{
		if (!bIsDirty)
		{
			return;
		}
		
		if (SubmissionWorkflow.CanSubmit())
		{
			SubmitChanges();
		}
		else if (!SubmissionWorkflow.OnSubmitOperationCompleted().IsBoundToObject(this))
		{
			SubmissionWorkflow.OnSubmitOperationCompleted().AddRaw(this, &FAutoSubmissionPolicy::ProcessAccumulatedChangesAndSubmit);
		}
	}

	void FAutoSubmissionPolicy::SubmitChanges()
	{
		using namespace UE::ConcertSyncClient::Replication;
		FCoreDelegates::OnEndFrame.RemoveAll(this);
			
		// Even though authority request is sent after server confirms stream change, the authority request is pre-built to avoid sending changes
		// the local client makes while we're waiting for the latent server responses.
		TOptional<FAuthorityChangeRequest> AuthorityChangeRequest = RequestBuilder.BuildAuthorityChange();
		TOptional<FChangeStreamRequest> StreamRequest = RequestBuilder.BuildStreamChange();
			
		// Should be true since a change caused OnEndFrame to be registered in the first place, but in THEORY changes could have been reverted in the same frame (e.g. via a transaction)
		if (AuthorityChangeRequest || StreamRequest)
		{
			SubmissionWorkflow.SubmitChanges({ StreamRequest, AuthorityChangeRequest });
		}
	}

	void FAutoSubmissionPolicy::OnChangesDetected()
	{
		bIsDirty = true;
	}
}
