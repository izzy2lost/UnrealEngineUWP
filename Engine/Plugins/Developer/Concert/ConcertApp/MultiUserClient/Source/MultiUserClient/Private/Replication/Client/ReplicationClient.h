// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Stream/LocalStreamChangeTracker.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;
class UMultiUserReplicationClientPreset;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
	enum class EReplicatedObjectChangeReason : uint8;
}

namespace UE::MultiUserClient
{
	/** Holds on to info about a local or remote client. */
	class FReplicationClient : public FNoncopyable
	{
	public:

		FReplicationClient(
			UMultiUserReplicationClientPreset& InSessionContent,
			TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer
			);

		UMultiUserReplicationClientPreset* GetClientContent() const { return ClientContentStorage; }
		TSharedRef<ConcertClientSharedSlate::IEditableObjectToPropertiesModel> GetClientEditModel() const { return LocalClientEditModel; }
		IClientStreamSynchronizer& GetStreamSynchronizer() const { return *StreamSynchronizer.Get(); } 
		
		const FLocalStreamChangeTracker& GetDiffer() const { return LocalClientStreamDiffer; }
		FLocalStreamChangeTracker& GetDiffer() { return LocalClientStreamDiffer; }

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
		
		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationClientPreset> ClientContentStorage;
		
		/** Keeps the client's state on the server in sync. */
		TUniquePtr<IClientStreamSynchronizer> StreamSynchronizer;
		
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
		FLocalStreamChangeTracker LocalClientStreamDiffer;

		/** Called when the data underlying the model has changed (and the UI needs to be refreshed). */
		FOnModelExternallyChanged OnModelExternallyChangedDelegate;

		// Respond to model changing
		void OnObjectsChanged(TConstArrayView<UObject*> Objects, TConstArrayView<FSoftObjectPath> SoftObjectPaths, ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason);
		void OnPropertiesChanged();
	};
}

