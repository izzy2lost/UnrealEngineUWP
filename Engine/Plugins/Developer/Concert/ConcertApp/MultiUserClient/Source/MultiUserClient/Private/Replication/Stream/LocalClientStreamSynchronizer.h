// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IToken.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Data/ObjectReplicationMap.h"
#include "Replication/Messages/ChangeStream.h"

#include "Async/Future.h"
#include "Misc/Attribute.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::ConcertSyncClient::Replication
{
	struct FChangeStreamResponse;
	struct FChangeStreamRequest;
}

namespace UE::MultiUserClient
{
	enum class EObjectWarningFlags
	{
		Ok = 0,
		MissingProperties = 1 << 0
	};
	ENUM_CLASS_FLAGS(EObjectWarningFlags);
	
	/**
	 * Knows of the local client's registered replication streams and builds a changelist. The changelist tracks the
	 * unconfirmed changes to the client's streams and is updates when the server confirms the change.
	 */
	class FLocalClientStreamSynchronizer : public FNoncopyable, public TSharedFromThis<FLocalClientStreamSynchronizer>
	{
	public:

		DECLARE_DELEGATE(FOnModifyReplicationMap);
		FLocalClientStreamSynchronizer(
			TSharedRef<IConcertSyncClient> InLocalClient,
			const FGuid& InLocalClientStreamId,
			TAttribute<FObjectReplicationMap*> InStreamWithInProgressChangesAttribute,
			FOnModifyReplicationMap InOnModifyReplicationMapDelegate
			);

		/**
		 * Clears and rebuilds CachedDeltaChange in response to the underlying StreamWithInProgressChangesAttribute having been changed.
		 * 
		 * This iterates through all elements, which is suboptimal. However, this is needed e.g. after an undo & redo command.
		 * In the future, we could implement a more efficient version that just responds to the relevant events (like adding an object).
		 * While this function does redundant work, it should really not be a big performance bog after all. Also it is called
		 * infrequently e.g. in response to a user pressing a button so we should be fine.
		 */
		void RefreshChangesCache();

		using FSubmissionResult = TPair<ConcertSyncClient::Replication::FChangeStreamRequest, ConcertSyncClient::Replication::FChangeStreamResponse>;
		/**
		 * Synchronizes the server with the locally made changes to the client stream.
		 * @note This future can execute on any thread.
		 */
		TFuture<FSubmissionResult> SubmitChanges(bool bRefreshChanges = false);
		
		/** Reverts the external stream StreamWithInProgressChangesAttribute to be the version registered on the server. */
		void RevertCachedChanges();

		/** @return Whether there are any changes that that can be submitted to the server (excludes those with warnings). */
		bool HasSubmittableLocalChanges() const;
		/** @return Whether there are any changes including those with warnings. */
		bool HasRevertableLocalChanges() const;
		
		/** @return Whether we're currently awaiting a response from the server for updating the stream. */
		bool IsChangeRequestInTransit() const;
		/** @return Whether it is ok to call SubmitChanges. */
		bool CanSubmitChanges() const { return HasSubmittableLocalChanges() && !IsChangeRequestInTransit(); }

		/** @return The warnings flags for the given ObjectPath. */
		EObjectWarningFlags GetObjectWarningFlags(const FSoftObjectPath& ObjectPath);
		/** @return Whether there are any warnings for any object. */
		bool HasAnyWarnings() const;

		enum class EObjectChangeType
		{
			NoChange,
			Added,
			Removed,
			PropertiesModified
		};
		/** @return Whether this object's local configuration differs from the server version. */
		EObjectChangeType GetObjectChanges(const FSoftObjectPath& Object) const;
		
		enum class EPropertyChangeType
		{
			NoChange,
			Added,
			Removed
		};
		/** @return Gets how the given property config differs from what is on the server. */
		EPropertyChangeType GetPropertyChanges(const FSoftObjectPath& Object, const FConcertPropertyChain& PropertyChain) const;

		/** Called when RevertCachedChanges is called. The UI should be refreshed. */
		DECLARE_MULTICAST_DELEGATE(FOnChangesReverted);
		FOnChangesReverted& OnChangesReverted_GameThread() { return OnChangesRevertedDelegate; }
		
		/** Called when a change request that was in transit was accepted by the server. */
		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnChangesAccepted, const FObjectReplicationMap& OldServerState, const ConcertSyncClient::Replication::FChangeStreamRequest& AcceptedRequest);
		FOnChangesAccepted& OnChangesAccepted_AnyThread() { return OnChangesAcceptedDelegate; }
		
	private:
		
		/** Referenced by the change requests to detect destruction of FLocalClientStreamSynchronizer */
		const TSharedRef<FToken> LifetimeToken = FToken::Make();
		
		/** Owning client. Used to send change requests to the server. */
		const TSharedRef<IConcertSyncClient> LocalClient;
		/** The ID of the local client's stream this FLocalClientStreamDiffer is managing. */
		const FGuid LocalClientStreamId;

		/**
		 * Represents ConfirmedServerState with changes made to it. These changes have not been sent to the server, yet.
		 * This is cleared when a change is sent to the server. 
		 */
		const TAttribute<FObjectReplicationMap*> StreamWithInProgressChangesAttribute;
		/**
		 * Represents what the local client thinks the replication map on the server currently looks like.
		 * This is updated every time the server confirms a change.
		 */
		FObjectReplicationMap ConfirmedServerState;

		struct FChangelist
		{
			TSet<FObjectInStreamID> ObjectsToRemove;
			TMap<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject> ObjectsToPut;
		};
		
		/**
		 * Represents a change to ConfirmedServerState that is currently in transit to the server.
		 * When a change is submitted to the server, ChangeInTransit becomes the difference between ConfirmedServerState and StreamWithInProgressChangesAttribute.
		 * When the server confirms these changes, they are applied to ConfirmedServerState.
		 */
		TOptional<FChangelist> ChangeInTransit;
		/**
		 * The changes that if applied to ConfirmedServerState would result in StreamWithInProgressChangesAttribute.
		 * This is updated every time the local client makes changes.
		 */
		FChangelist CachedDeltaChange;

		/** These objects have warnings that prevent them from being set to the server (missing data, unsupported class, etc.). */
		TMap<FSoftObjectPath, EObjectWarningFlags> ObjectsWithWarnings;
		
		/** Called when StreamWithInProgressChangesAttribute is about to be modified. */
		FOnModifyReplicationMap OnModifyReplicationMapDelegate;
		/** Called when StreamWithInProgressChangesAttribute was modified by this FLocalClientStreamDiffer. */
		FOnChangesReverted OnChangesRevertedDelegate;
		/** Called when a change request that was in transit was accepted by the server. */
		FOnChangesAccepted OnChangesAcceptedDelegate;

		FChangelist DiffChanges() const
		{
			const FObjectReplicationMap* Map = StreamWithInProgressChangesAttribute.Get();
			return Map ? DiffChanges(LocalClientStreamId, ConfirmedServerState, *Map) : FChangelist{};
		}
		/** Builds the changelist to get from Base to Changed. */
		static FChangelist DiffChanges(const FGuid& StreamId, const FObjectReplicationMap& Base, const FObjectReplicationMap& Changed);

		/** Updates ObjectsWithWarnings. */
		void RefreshWarnings();
		
		/** Builds a change request based on what's registered on the server. */
		ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest() const;
		enum class EChangeRequestType
		{
			/** When NeedsToCreateNewStreamNextRequest returns true */
			CreateNewStream,
			UpdateExistingStream
		};
		/** Converts a changelist to. */
		static ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest_CreateNewStream(const FGuid& StreamId, const FChangelist& FromChangelist);
		static ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest_UpdateExistingStream(FChangelist FromChangelist);
		/**
		 * The stream may have to be created first. This is the case either if
		 *	1. this is the first time sending to the server, or
		 *	2. the client created objects and then removed them all again (the server destroys the stream in ths case).
		 */
		EChangeRequestType ComputeNextRequestType() const;

		/** Updates the server state after the server has answered a request. */
		void UpdateConfirmedServerState(
			const ConcertSyncClient::Replication::FChangeStreamRequest& Request,
			const FChangelist& ChangeThatWasInTransit
			);
	};
}

