// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

enum class EBreakBehavior : uint8;

namespace UE::MultiUserClient::Replication
{
	class FOnlineClient;
	class FOnlineClientManager;
	class IClientSelectionModel;

	/** Checks whether clients accept remote changes and categorizes them into read-only and writable. */
	class FMultiStreamModel : public ConcertSharedSlate::IEditableMultiReplicationStreamModel
	{
	public:
		
		FMultiStreamModel(IClientSelectionModel& InClientSelectionModel, FOnlineClientManager& InClientManager);
		
		const TSet<const FOnlineClient*>& GetCachedReadOnlyClients() const { return CachedReadOnlyClients; }
		const TSet<const FOnlineClient*>& GetCachedWritableClients() const { return CachedWritableClients; }
		void ForEachClient(TFunctionRef<EBreakBehavior(const FOnlineClient*)> ProcessClient) const;

		//~ Begin IEditableMultiReplicationStreamModel Interface
		virtual TSet<TSharedRef<ConcertSharedSlate::IReplicationStreamModel>> GetReadOnlyStreams() const override;
		virtual TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> GetEditableStreams() const override;
		virtual FOnStreamExternallyChanged& OnStreamExternallyChanged() override { return OnReadOnlyStreamChangedDelegate; }
		virtual FOnStreamSetChanged& OnStreamSetChanged() override { return OnStreamSetChangedDelegate; }
		//~ End IEditableMultiReplicationStreamModel Interface

	private:
		
		/** Gets all clients to display and informs when they change. */
		IClientSelectionModel& ClientSelectionModel;
		/** Used to obtain a list of clients for unsubscribing */
		FOnlineClientManager& ClientManager;

		TSet<const FOnlineClient*> CachedReadOnlyClients;
		TSet<const FOnlineClient*> CachedWritableClients;

		FOnStreamExternallyChanged OnReadOnlyStreamChangedDelegate;
		FOnStreamSetChanged OnStreamSetChangedDelegate;
		
		void RebuildStreamsSets();

		/** Handle read-only streams changing */
		void OnStreamExternallyChanged(TWeakPtr<ConcertSharedSlate::IEditableReplicationStreamModel> ChangedStream);
	};
}

