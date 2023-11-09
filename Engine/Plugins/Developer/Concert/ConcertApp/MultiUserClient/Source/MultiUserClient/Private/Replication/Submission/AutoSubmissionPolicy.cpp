// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSubmissionPolicy.h"

#include "ISubmissionWorkflow.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"

#include "Misc/CoreDelegates.h"
#include "Replication/Authority/AuthorityChangeTracker.h"

namespace UE::MultiUserClient
{
	FAutoSubmissionPolicy::FAutoSubmissionPolicy(
		ISubmissionWorkflow& InSubmissionWorkflow,
		ConcertClientSharedSlate::IEditableReplicationStreamModel& InStreamEditorModel,
		FAuthorityChangeTracker& InAuthorityChangeTracker
		)
		: SubmissionWorkflow(InSubmissionWorkflow)
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
	}

	void FAutoSubmissionPolicy::OnChangesDetected()
	{
		if (!FCoreDelegates::OnEndFrame.IsBoundToObject(this))
		{
			FCoreDelegates::OnEndFrame.AddRaw(this, &FAutoSubmissionPolicy::OnEndFrame);
		}
	}

	void FAutoSubmissionPolicy::OnEndFrame()
	{
		if (SubmissionWorkflow.CanSubmit())
		{
			FCoreDelegates::OnEndFrame.RemoveAll(this);
			SubmissionWorkflow.SubmitChanges();
		}
	}
}
