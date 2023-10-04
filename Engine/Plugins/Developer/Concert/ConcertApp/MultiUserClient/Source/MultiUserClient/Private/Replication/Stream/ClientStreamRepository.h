// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "UObject/GCObject.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
}

namespace UE::MultiUserClient
{
	class FLocalClientStreamSynchronizer;

	/** Stores all clients' streams in a UMultiUserReplicationSessionPreset and keeps it in sync with the asset. */
	class FClientStreamRepository
		: public FGCObject
		, public FNoncopyable
	{
	public:

		FClientStreamRepository(TSharedRef<IConcertSyncClient> InClient);
		
		UMultiUserReplicationSessionPreset* GetSessionContent() const { return SessionContent; }
		UMultiUserReplicationClientPreset* GetLocalClientContent() const { return LocalClientContent; }
		TSharedRef<ConcertClientSharedSlate::IEditableObjectToPropertiesModel> GetLocalClientEditModel() const { return LocalClientEditModel; }
		
		const FLocalClientStreamSynchronizer& GetDiffer() const { return LocalClientStreamDiffer.Get(); }
		FLocalClientStreamSynchronizer& GetDiffer() { return LocalClientStreamDiffer.Get(); }

		/**
		 * Called when the data underlying the model has changed externally. Since the change was not caused by the model,
		 * its events, like IEditableObjectToPropertiesModel::OnObjectsChanged, were not called.
		 * 
		 * Subscribers are intended to call IReplicationEditorView::Refresh() in response.
		 * 
		 * Examples: Remote client changed their stream, remote client joined with streams,
		 * or the local client reverted a change that was rejected by the server.
		 */
		DECLARE_MULTICAST_DELEGATE(FOnModelChanged);
		FOnModelChanged& OnModelChanged_GameThread() { return OnModelChangedDelegate; }
		
		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FReplicationStreamSynchronizer"); }
		//~ End FGCObject Interface
		
	private:
		
		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationSessionPreset> SessionContent;
		/** Data for the local client. Also part of SessionContent->ClientPresets. */
		TObjectPtr<UMultiUserReplicationClientPreset> LocalClientContent;

		/**
		 * Used to detect changes made to the local client.
		 
		 * The SessionContent UObject must be kept alive as long as this model lives. 
		 * It should be noted that the UI keeps a strong reference to this model.
		 * The UI is destroyed right after this FClientStreamRepository.
		 * @see FMultiUserReplicationManager::OnLeaveSession
		 */
		TSharedRef<ConcertClientSharedSlate::IEditableObjectToPropertiesModel> LocalClientEditModel;

		/** Keeps track of the server's state of the local client's streams and handles changing them. */
		TSharedRef<FLocalClientStreamSynchronizer> LocalClientStreamDiffer;

		/** Called when the data underlying the model has changed and the UI needs to be refreshed. */
		FOnModelChanged OnModelChangedDelegate;
		
		void OnObjectsChanged(TConstArrayView<UObject*> Objects, TConstArrayView<FSoftObjectPath> SoftObjectPaths, ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason);
		void OnPropertiesChanged();
	};
}

