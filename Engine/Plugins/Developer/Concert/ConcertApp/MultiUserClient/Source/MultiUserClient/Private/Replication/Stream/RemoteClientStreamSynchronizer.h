// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientStreamSynchronizer.h"
#include "Replication/Data/ObjectReplicationMap.h"

#include "Async/Future.h"
#include "Misc/Attribute.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	class FRegularQueryService;
	
	/**
	 * Tracks the state of a remote client.
	 * Queries the client's state in regular intervals.
	 */
	class FRemoteClientStreamSynchronizer : public IClientStreamSynchronizer, public FNoncopyable
	{
	public:
		
		FRemoteClientStreamSynchronizer(const FGuid& RemoteEndpointId, FRegularQueryService& InQueryService);
		virtual ~FRemoteClientStreamSynchronizer() override;

		//~ Begin IClientStreamSynchronizer Interface
		virtual TFuture<FSubmitChangesResult> SubmitChanges(const FStreamChangelist& Changelist) override;
		virtual bool CanMakeSubmitRequest() const override;
		virtual FGuid GetStreamId() const override;
		virtual const FObjectReplicationMap& GetServerState() const override { return LastKnownServerState; }
		virtual FOnChangesAccepted& OnChangesAccepted() override { return OnChangesAcceptedDelegate; }
		virtual FOnServerStateChanged& OnServerStateSynched() override { return OnServerStateSynchedDelegate; }
		//~ End IClientStreamSynchronizer Interface

	private:

		/** Queries the server in regular intervals. This services outlives our object. */
		FRegularQueryService& QueryService;
		/** Used to unregister upon destruction. */
		const FDelegateHandle QueryStreamHandle; 

		/** Represents what the local client thinks the replication map on the server currently looks like. */
		FObjectReplicationMap LastKnownServerState;
		
		/** Called when a change request that was in transit was accepted by the server. */
		FOnChangesAccepted OnChangesAcceptedDelegate;
		/** Event executed when the result of GetServerState has been synched. */
		FOnServerStateChanged OnServerStateSynchedDelegate;

		/** Called in regular intervals with the contents of the remote client. */
		void HandleStreamQuery(const TArray<FSharedReplicationStreamDescription>& Streams);
	};
}

