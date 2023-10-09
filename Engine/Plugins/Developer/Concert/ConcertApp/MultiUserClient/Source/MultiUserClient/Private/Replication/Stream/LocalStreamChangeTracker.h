// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientStreamSynchronizer.h"
#include "Replication/Data/ObjectReplicationMap.h"

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
	class FLocalStreamChangeTracker : public FNoncopyable
	{
	public:

		DECLARE_DELEGATE(FOnModifyReplicationMap);
		FLocalStreamChangeTracker(
			TSharedRef<IClientStreamSynchronizer> InStreamSynchronizer,
			TAttribute<FObjectReplicationMap*> InStreamWithInProgressChangesAttribute,
			FOnModifyReplicationMap InOnModifyReplicationMapDelegate
			);
		~FLocalStreamChangeTracker();

		/**
		 * Clears and rebuilds CachedDeltaChange in response to the underlying StreamWithInProgressChangesAttribute having been changed.
		 * 
		 * This iterates through all elements, which is suboptimal. However, this is needed e.g. after an undo & redo command.
		 * In the future, we could implement a more efficient version that just responds to the relevant events (like adding an object).
		 * While this function does redundant work, it should really not be a big performance bog after all. Also it is called
		 * infrequently e.g. in response to a user pressing a button so we should be fine.
		 */
		void RefreshChangesCache();

		/**
		 * Synchronizes the server with the locally made changes to the client stream.
		 * @note This future can execute on any thread.
		 */
		TFuture<FSubmitChangesResult> SubmitChanges(bool bRefreshChanges = false);
		
		/** Reverts the external stream StreamWithInProgressChangesAttribute to be the version registered on the server. */
		void RevertCachedChanges();

		/** @return Whether there are any changes that that can be submitted to the server (excludes those with warnings). */
		bool HasSubmittableLocalChanges() const;
		/** @return Whether there are any changes including those with warnings. */
		bool HasRevertableLocalChanges() const;
		
		/** @return Whether we're currently awaiting a response from the server for updating the stream. */
		bool CanMakeSubmitNetworkRequest() const;
		/** @return Whether it is ok to call SubmitChanges. */
		bool CanSubmitChanges() const { return HasSubmittableLocalChanges() && CanMakeSubmitNetworkRequest(); }

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
		
	private:

		/** Keeps track of the edited stream's server state. */
		TSharedRef<IClientStreamSynchronizer> StreamSynchronizer;

		/**
		 * Represents ConfirmedServerState with changes made to it. These changes have not been sent to the server, yet.
		 * This is cleared when a change is sent to the server. 
		 */
		const TAttribute<FObjectReplicationMap*> StreamWithInProgressChangesAttribute;
		
		/**
		 * The changes that if applied to ConfirmedServerState would result in StreamWithInProgressChangesAttribute.
		 * This is updated every time the local client makes changes.
		 */
		FStreamChangelist CachedDeltaChange;

		/** These objects have warnings that prevent them from being set to the server (missing data, unsupported class, etc.). */
		TMap<FSoftObjectPath, EObjectWarningFlags> ObjectsWithWarnings;
		
		/** Called when StreamWithInProgressChangesAttribute is about to be modified. */
		FOnModifyReplicationMap OnModifyReplicationMapDelegate;
		/** Called when StreamWithInProgressChangesAttribute was modified by this FLocalClientStreamDiffer. */
		FOnChangesReverted OnChangesRevertedDelegate;

		FStreamChangelist DiffChanges() const
		{
			const FObjectReplicationMap* Map = StreamWithInProgressChangesAttribute.Get();
			return Map ? DiffChanges(StreamSynchronizer->GetStreamId(), StreamSynchronizer->GetServerState(), *Map) : FStreamChangelist{};
		}
		/** Builds the changelist to get from Base to Changed. */
		static FStreamChangelist DiffChanges(const FGuid& StreamId, const FObjectReplicationMap& Base, const FObjectReplicationMap& Changed);

		/** Updates ObjectsWithWarnings. */
		void RefreshWarnings();
	};
}

