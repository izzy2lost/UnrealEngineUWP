// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Authority/IClientAuthoritySynchronizer.h"
#include "Replication/Stream/IClientStreamSynchronizer.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Submission/AutoSubmissionPolicy.h"
#include "Replication/Submission/ChangeRequestBuilder.h"
#include "Replication/Submission/ISubmissionWorkflow.h"

#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class UMultiUserReplicationClientPreset;

namespace UE::ConcertClientSharedSlate
{
	class IEditableReplicationStreamModel;
	enum class EReplicatedObjectChangeReason : uint8;
}

namespace UE::MultiUserClient
{
	class ISubmissionWorkflow;
	
	/**
	 * Holds on to shared info about a local or remote client.
	 * This class' responsibility is to initialize all systems that exist for the life time of a client in a session.
	 */
	class FReplicationClient : public FNoncopyable
	{
	public:
		
		FReplicationClient(
			const FGuid& EndpointId,
			FGlobalAuthorityCache& InAuthorityCache,
			UMultiUserReplicationClientPreset& InSessionContent,
			TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
			TUniquePtr<IClientAuthoritySynchronizer> InAuthoritySynchronizer,
			TUniquePtr<ISubmissionWorkflow> InSubmissionWorkflow
			);
		~FReplicationClient();

		UMultiUserReplicationClientPreset* GetClientContent() const { return ClientContentStorage; }
		TSharedRef<ConcertClientSharedSlate::IEditableReplicationStreamModel> GetClientEditModel() const { return LocalClientEditModel; }
		IClientStreamSynchronizer& GetStreamSynchronizer() const { return *StreamSynchronizer.Get(); }
		IClientAuthoritySynchronizer& GetAuthoritySynchronizer() const { return *AuthoritySynchronizer.Get(); }
		
		const FStreamChangeTracker& GetStreamDiffer() const { return LocalClientStreamDiffer; }
		FStreamChangeTracker& GetStreamDiffer() { return LocalClientStreamDiffer; }
		
		const FAuthorityChangeTracker& GetAuthorityDiffer() const { return LocalAuthorityDiffer; }
		FAuthorityChangeTracker& GetAuthorityDiffer() { return LocalAuthorityDiffer; }
		
		const ISubmissionWorkflow& GetSubmissionWorkflow() const { return *SubmissionWorkflow; }
		ISubmissionWorkflow& GetSubmissionWorkflow() { return *SubmissionWorkflow; }

		/** @return The endpoint ID of this client in the Concert session. */
		const FGuid& GetEndpointId() const { return EndpointId; }
		/** @return Whether it is allowed to edit the stream and authority for this client. */
		bool AllowsEditing() const;
		
		bool operator==(const FReplicationClient& Client) const { return GetEndpointId() == Client.GetEndpointId(); }

		DECLARE_MULTICAST_DELEGATE(FOnModelExternallyChanged);
		/**
		 * Broadcast when the data underlying the model has changed for any reason:
		 * - Edited directly by client
		 * - Changed by transaction
		 * - Server state changed
		 */
		FOnModelExternallyChanged& OnModelChanged() { return OnModelChangedDelegate; }
		
	private:

		/** This client's Concert Endpoint ID. */
		const FGuid EndpointId;
		
		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationClientPreset> ClientContentStorage;
		
		/** Keeps the client's stream state on the server in sync. */
		TUniquePtr<IClientStreamSynchronizer> StreamSynchronizer;
		/** Keeps the client's authority state on the server in sync.*/
		TUniquePtr<IClientAuthoritySynchronizer> AuthoritySynchronizer;
		/** Handles the logic of submitting and reverting for this client. */
		TUniquePtr<ISubmissionWorkflow> SubmissionWorkflow;
		
		/**
		 * Used to detect changes made to the client's config by the local editor.
		 * Those changes can later be applied to the client.
		 * 
		 * The SessionContent UObject must be kept alive as long as this model lives. 
		 * It should be noted that the UI keeps a strong reference to this model.
		 * The UI is destroyed right after this FClientStreamRepository.
		 * @see FMultiUserReplicationManager::OnLeaveSession
		 */
		TSharedRef<ConcertClientSharedSlate::IEditableReplicationStreamModel> LocalClientEditModel;
		
		/** Tracks changes made to server's state of the client's streams and prepares to upload them using StreamSynchronizer. */
		FStreamChangeTracker LocalClientStreamDiffer;
		/** Tracks changes made to the client's authority state. */
		FAuthorityChangeTracker LocalAuthorityDiffer;
		
		/** Shared logic for building stream and authority change requests based on local change made. */
		FChangeRequestBuilder ChangeRequestBuilder;
		/** Automatically submits changes as they are made by the user. */
		FAutoSubmissionPolicy AutoSubmissionPolicy;

		/**
		 * Broadcast when the data underlying the model has changed for any reason:
		 * - Edited directly by client
		 * - Changed by transaction
		 * - Server state changed
		 */
		FOnModelExternallyChanged OnModelChangedDelegate;

		struct FDeferredOnModelChangedData
		{
			TSet<TWeakObjectPtr<UObject>> AccumulatedAddedObjects;
		};
		TOptional<FDeferredOnModelChangedData> DeferredOnModelChangedData;

		// Respond to model changing
		void OnObjectsChanged(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason);
		void OnPropertiesChanged();

		/** Defers rebuilding operations, such as refreshing state and calling OnModelChanged, in case there are multiple changes in the same frame. */
		void DeferOnModelChanged() { DeferOnModelChanged({}); }
		void DeferOnModelChanged(TConstArrayView<UObject*> AddedObjects);
		/** Processes all changes that have happened to the stream this frame. */
		void ProcessOnModelChanged();
		
		/** Takes authority over newly added objects for better UX */
		void TakeAuthorityOverNewlyAddedObjects(const FDeferredOnModelChangedData& ChangeData);
		
		/** Removes authority if request fails */
		void OnAuthoritySubmissionCompleted(const FSubmitAuthorityChangesRequest& Request, const FSubmitAuthorityChangesResponse& Response);
	};
}

