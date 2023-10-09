// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientStreamSynchronizer.h"
#include "Replication/IToken.h"
#include "Replication/Data/ObjectReplicationMap.h"

#include "Async/Future.h"
#include "Misc/Attribute.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	/**
	 * Knows of the local client's registered replication streams and builds a changelist. The changelist tracks the
	 * unconfirmed changes to the client's streams and is updates when the server confirms the change.
	 */
	class FLocalClientStreamSynchronizer : public IClientStreamSynchronizer, public FNoncopyable
	{
	public:

		FLocalClientStreamSynchronizer(TSharedRef<IConcertSyncClient> InLocalClient, const FGuid& InLocalClientStreamId);

		//~ Begin IClientStreamSynchronizer Interface
		virtual TFuture<FSubmitChangesResult> SubmitChanges(const FStreamChangelist& Changelist) override;
		virtual bool CanMakeSubmitRequest() const override { return !ChangeInTransit.IsSet(); }
		virtual FGuid GetStreamId() const override { return LocalClientStreamId; }
		virtual const FObjectReplicationMap& GetServerState() const override { return ConfirmedServerState; }
		virtual FOnChangesAccepted& OnChangesAccepted_AnyThread() override { return OnChangesAcceptedDelegate; }
		virtual FOnServerStateChanged& OnServerStateSynched_AnyThread() override { return OnServerStateSynchedDelegate; }
		//~ End IClientStreamSynchronizer Interface

	private:
		
		/** Referenced by the change requests to detect destruction of FLocalClientStreamSynchronizer */
		const TSharedRef<FToken> LifetimeToken = FToken::Make();
		
		/** Owning client. Used to send change requests to the server. */
		const TSharedRef<IConcertSyncClient> LocalClient;
		/** The ID of the local client's stream this FLocalClientStreamDiffer is managing. */
		const FGuid LocalClientStreamId;

		/**
		 * Represents what the local client thinks the replication map on the server currently looks like.
		 * This is updated every time the server confirms a change.
		 */
		FObjectReplicationMap ConfirmedServerState;
		
		/**
		 * Represents a change to ConfirmedServerState that is currently in transit to the server.
		 * When a change is submitted to the server, ChangeInTransit becomes the difference between ConfirmedServerState and StreamWithInProgressChangesAttribute.
		 * When the server confirms these changes, they are applied to ConfirmedServerState.
		 */
		TOptional<FStreamChangelist> ChangeInTransit;
		
		/** Called when a change request that was in transit was accepted by the server. */
		FOnChangesAccepted OnChangesAcceptedDelegate;
		/** Event executed when the result of GetServerState has been synched. */
		FOnServerStateChanged OnServerStateSynchedDelegate;
		
		/** Builds a change request based on what's registered on the server. */
		ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest(const FStreamChangelist& Changelist) const;
		enum class EChangeRequestType
		{
			/** When NeedsToCreateNewStreamNextRequest returns true */
			CreateNewStream,
			UpdateExistingStream
		};
		/** Converts a changelist to. */
		static ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest_CreateNewStream(const FGuid& StreamId, const FStreamChangelist& FromChangelist);
		static ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest_UpdateExistingStream(FStreamChangelist FromChangelist);
		/**
		 * The stream may have to be created first. This is the case either if
		 *	1. this is the first time sending to the server, or
		 *	2. the client created objects and then removed them all again (the server destroys the stream in ths case).
		 */
		EChangeRequestType ComputeNextRequestType() const;

		/** Updates the server state after the server has answered a request. */
		void UpdateConfirmedServerState(
			const ConcertSyncClient::Replication::FChangeStreamRequest& Request,
			const FStreamChangelist& ChangeThatWasInTransit
			);
	};
}

