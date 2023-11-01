// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Authority/IClientAuthoritySynchronizer.h"
#include "Replication/Stream/IClientStreamSynchronizer.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Submission/AutoSubmissionPolicy.h"
#include "Replication/Submission/ISubmissionWorkflow.h"

#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class UMultiUserReplicationClientPreset;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
	enum class EReplicatedObjectChangeReason : uint8;
}

namespace UE::MultiUserClient
{
	class ISubmissionWorkflow;
	
	/** Holds on to info about a local or remote client. */
	class FReplicationClient : public FNoncopyable
	{
	public:
		
		/** Indirection for creating ISubmissionWorkflow because some ISubmissionWorkflow implementations needs members constructed in FReplicationClient. */
		using FMakeSubmissionWorkflow = TUniquePtr<ISubmissionWorkflow>(FStreamChangeTracker&, FAuthorityChangeTracker&, IClientStreamSynchronizer&);

		FReplicationClient(
			const FGuid& EndpointId,
			UMultiUserReplicationClientPreset& InSessionContent,
			TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
			TUniquePtr<IClientAuthoritySynchronizer> InAuthoritySynchronizer,
			TFunctionRef<FMakeSubmissionWorkflow> MakeSubmissionWorkflowFunc
			);

		UMultiUserReplicationClientPreset* GetClientContent() const { return ClientContentStorage; }
		TSharedRef<ConcertClientSharedSlate::IEditableObjectToPropertiesModel> GetClientEditModel() const { return LocalClientEditModel; }
		IClientStreamSynchronizer& GetStreamSynchronizer() const { return *StreamSynchronizer.Get(); }
		IClientAuthoritySynchronizer& GetAuthoritySynchronizer() const { return *AuthoritySynchronizer.Get(); }
		
		const FStreamChangeTracker& GetStreamDiffer() const { return LocalClientStreamDiffer; }
		FStreamChangeTracker& GetStreamDiffer() { return LocalClientStreamDiffer; }
		
		const FAuthorityChangeTracker& GetAuthorityDiffer() const { return LocalAuthorityDiffer; }
		FAuthorityChangeTracker& GetAuthorityDiffer() { return LocalAuthorityDiffer; }
		
		const ISubmissionWorkflow& GetSubmissionWorkflow() const { return *SubmissionWorkflow; }
		ISubmissionWorkflow& GetSubmissionWorkflow() { return *SubmissionWorkflow; }

		const FGuid& GetEndpointId() const { return EndpointId; }

		/**
		 * Called when the data underlying the model has changed externally. Since the change was not caused by the model,
		 * its events, like IEditableObjectToPropertiesModel::OnObjectsChanged, were not called.
		 * 
		 * Subscribers are intended to call IReplicationStreamEditor::Refresh() in response.
		 * 
		 * Examples: Remote client changed their stream, remote client joined with streams,
		 * or the local client reverted a change that was rejected by the server.
		 */
		DECLARE_MULTICAST_DELEGATE(FOnModelExternallyChanged);
		FOnModelExternallyChanged& OnModelExternallyChanged() { return OnModelExternallyChangedDelegate; }
		
	private:

		/** This client's Concert Endpoint ID. */
		const FGuid EndpointId;
		
		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationClientPreset> ClientContentStorage;
		
		/** Keeps the client's stream state on the server in sync. */
		TUniquePtr<IClientStreamSynchronizer> StreamSynchronizer;
		/** Keeps the client's authority state on the server in sync.*/
		TUniquePtr<IClientAuthoritySynchronizer> AuthoritySynchronizer;
		
		/**
		 * Used to detect changes made to the client's config by the local editor.
		 * Those changes can later be applied to the client.
		 * 
		 * The SessionContent UObject must be kept alive as long as this model lives. 
		 * It should be noted that the UI keeps a strong reference to this model.
		 * The UI is destroyed right after this FClientStreamRepository.
		 * @see FMultiUserReplicationManager::OnLeaveSession
		 */
		TSharedRef<ConcertClientSharedSlate::IEditableObjectToPropertiesModel> LocalClientEditModel;
		
		/** Tracks changes made to server's state of the client's streams and prepares to upload them using StreamSynchronizer. */
		FStreamChangeTracker LocalClientStreamDiffer;
		/** Tracks changes made to the client's authority state. */
		FAuthorityChangeTracker LocalAuthorityDiffer;
		
		/** Handles the logic of submitting and reverting for this client. */
		TUniquePtr<ISubmissionWorkflow> SubmissionWorkflow;
		/** Automatically submits changes as they are made by the user. */
		FAutoSubmissionPolicy AutoSubmissionPolicy;

		/** Called when the data underlying the model has changed (and the UI needs to be refreshed). */
		FOnModelExternallyChanged OnModelExternallyChangedDelegate;

		// Respond to model changing
		void OnObjectsChanged(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason);
		void OnPropertiesChanged();
	};
}

