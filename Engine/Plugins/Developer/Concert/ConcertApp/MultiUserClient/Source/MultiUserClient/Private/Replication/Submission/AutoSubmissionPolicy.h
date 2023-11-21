// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Templates/UnrealTemplate.h"

namespace UE::MultiUserClient
{
	class FChangeRequestBuilder;
}

namespace UE::ConcertClientSharedSlate
{
	class IEditableReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class IClientStreamSynchronizer;
	class ISubmissionWorkflow;
	class FAuthorityChangeTracker;
	class FStreamChangeTracker;
	
	/**
	 * Automatically submits stream and authority changes as the user makes them.
	 * If a change is in progress when the change is made, the changes are queued and sent once the pending submission is done.
	 *
	 * This policy accumulates all changes until the end of frame and then sends them. This means multiple changes are made in the same frame,
	 * only one request will be sent containing all changes.
	 */
	class FAutoSubmissionPolicy : public FNoncopyable
	{
	public:
		
		FAutoSubmissionPolicy(
			ISubmissionWorkflow& InSubmissionWorkflow,
			const FChangeRequestBuilder& InRequestBuilder,
			ConcertClientSharedSlate::IEditableReplicationStreamModel& InStreamEditorModel,
			FAuthorityChangeTracker& InAuthorityChangeTracker
			);
		~FAutoSubmissionPolicy();
		
	private:

		/** Handles performing the submission */
		ISubmissionWorkflow& SubmissionWorkflow;
		/** Used to building the requests that are passed to SubmissionWorkflow. */
		const FChangeRequestBuilder& RequestBuilder;

		/** Informs us when the stream is structurally changed by the user. */
		ConcertClientSharedSlate::IEditableReplicationStreamModel& StreamEditorModel;
		/** Informs us when authority is changed by the user. */
		FAuthorityChangeTracker& AuthorityChangeTracker;
		
		void OnObjectsChanged(TArrayView<UObject* const>, TArrayView<const FSoftObjectPath>, ConcertClientSharedSlate::EReplicatedObjectChangeReason) { OnChangesDetected(); }
		void OnChangesDetected();

		/** Checks whether any changes were made last frame and submits a change request if so. */
		void OnEndFrame();
	};
}

