// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationManagerState.h"
#include "Replication/Processing/ClientReplicationDataCollector.h"
#include "Replication/Processing/ClientReplicationDataQueuer.h"

namespace UE::ConcertSyncCore
{
	class FObjectReplicationReceiver;
}

namespace UE::ConcertSyncCore
{
	class FObjectReplicationSender;
}

class IConcertClientSession;

namespace UE::ConcertSyncClient::Replication
{
	class FObjectReplicationApplierProcessor;
	class FObjectReplicationSender;
	
	/**
	 * State for when the client has successfully completed a replication handshake.
	 *
	 * Every tick this state tries to
	 *	- collect data and sends it to the server
	 *	- process received data and applies it
	 */
	class FReplicationManagerState_Connected : public FReplicationManagerState
	{
	public:

		FReplicationManagerState_Connected(
			TSharedRef<IConcertClientSession> LiveSession,
			IConcertClientReplicationBridge* ReplicationBridge,
			TArray<FReplicationStreamDescription> StreamDescriptions,
			FReplicationManager& Owner
			);
		virtual ~FReplicationManagerState_Connected() override;

		//~ Begin IConcertClientReplicationManager Interface
		virtual TFuture<FJoinReplicatedSessionResult> JoinReplicationSession(FJoinReplicatedSessionArgs Args) override;
		virtual void LeaveReplicationSession() override;
		virtual bool CanJoin() override { return false; }
		virtual bool IsConnectedToReplicationSession() override { return true; }
		virtual EStreamEnumerationResult ForEachRegisteredStream(TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback) const override;
		virtual TFuture<FAuthorityChangeResponse> RequestAuthorityChange(FAuthorityChangeRequest Args) override;
		virtual TFuture<FClientQueryResponse> QueryClientInfo(FClientQueryRequest Args) override;
		virtual TFuture<FChangeStreamResponse> ChangeStream(FChangeStreamRequest Args) override;
		virtual EAuthorityEnumerationResult ForEachClientOwnedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)> Callback) const override;
		virtual TSet<FGuid> GetClientOwnedStreamsForObject(const FSoftObjectPath& ObjectPath) const override;
		//~ End IConcertClientReplicationManager Interface

	private:

		/** Passed to FReplicationManagerState_Disconnected */
		const TSharedRef<IConcertClientSession> LiveSession;
		/** Passed to FReplicationManagerState_Disconnected */
		IConcertClientReplicationBridge* const ReplicationBridge;
		/** The streams this client has registered with the server. */
		TArray<FReplicationStreamDescription> RegisteredStreams;
		
		/** The format this client will use for sending & receiving data. */
		const TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat;

		// Sending
		/** Used as source of replication data. */
		const TSharedRef<FClientReplicationDataCollector> ReplicationDataSource;
		/** Sends data collected by ReplicationDataSource to the server. */
		const TSharedRef<ConcertSyncCore::FObjectReplicationSender> Sender;

		// Receiving
		/** Stores data received by Receiver until it is consumed by ReceivedReplicationQueuer. */
		const TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReceivedDataCache;
		/** Receives data from remote endpoints via message bus.  */
		const TSharedRef<ConcertSyncCore::FObjectReplicationReceiver> Receiver;
		/** Queues data until is can be processed. */
		const TSharedRef<FClientReplicationDataQueuer> ReceivedReplicationQueuer;
		/** Processes data from ReceivedReplicationQueuer once we tick. */
		const TSharedRef<FObjectReplicationApplierProcessor> ReplicationApplier;

		using FTickTask = void(FReplicationManagerState_Connected::*)(float TimeBudget);
		/**
		 * We have two tasks that need to be performed each tick: Collecting data to send and applying received data.
		 * In order that no task starves the other, we alternate which one we start with.
		 *
		 * Example assuming time budget of 0.1s (unrealistic time budget):
		 * Tick 1: TickSender takes 0.8s and finishes. TickReceiver has 0.2s but cannot finish all work.
		 * Tick 2: TickReceiver finishes work taking 0.7s. TickSender gets the remaining 0.3s.
		 * Tick 3. TickSender starts again followed by TickSender ...
		 */
		const TArray<FTickTask, TInlineAllocator<2>> AlternatingTickTasks { &FReplicationManagerState_Connected::TickSender, &FReplicationManagerState_Connected::TickReceiver };
		/** The first index to process next tick. */
		int32 NextTickTaskIndex = 0;

		//~ Begin FReplicationManagerState Interface
		virtual void OnEnterState() override;
		//~ End FReplicationManagerState Interface

		/**
		 * Ticks this client.
		 * 
		 * This processes:
		 *  - data that is to be sent
		 *  - data that was received
		 *
		 * The tasks have a time budget so that the frame rate remains stable.
		 * It is configured in the project settings TODO: Add config
		 */
		void Tick(IConcertClientSession& Session, float DeltaTime);

		/** Collects and sends data to the server. */
		void TickSender(float TimeBudget);
		/** Processes received data and serializes UObjects. */
		void TickReceiver(float TimeBudget);
		
		/** Updates replicated objects affected by the change request. */
		void UpdateReplicatedObjectsAfterStreamChange(const FChangeStreamRequest& Request, const FConcertReplication_ChangeStream_Response& Response);
		void HandleRemovingReplicatedObjects(const FChangeStreamRequest& Request) const;

		/**
		 * Updates the objects which should be replicated after changing authority.
		 * 
		 * @note Request is accepted as && because this function rewrites its memory when looking at rejections.
		 * Since the request was already sent to the server it is assumed the request can just contain trash after.
		 */
		void UpdateReplicatedObjectsAfterAuthorityChange(FAuthorityChangeRequest&& Request, const FConcertReplication_ChangeAuthority_Response& Response) const;
		void HandleReleasingReplicatedObjects(const FAuthorityChangeRequest& Request) const;
	};
}
