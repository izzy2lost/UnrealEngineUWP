// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessages.h"
#include "Misc/EBreakBehavior.h"
#include "Templates/Function.h"

class IConcertSession;

struct FConcertChangeAuthority_Response;
struct FConcertChangeAuthority_Request;
struct FConcertPropertySelection;
struct FConcertSessionContext;
struct FObjectInStreamID;
struct FReplicatedObjectId;
struct FReplicationStreamDescription;

namespace UE::ConcertSyncServer::Replication
{
	/**
	 * Callbacks FAuthorityManager requires to function correctly.
	 * Exists to make FAuthorityManager independent from all the other systems and allows mocking in tests.
	 */
	class IAuthorityManagerGetters
	{
	public:

		/** Provides a way to extract all streams registered to a given client. */
		virtual void ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback) = 0;

		/** Iterates through all clients have registered to send any data. */
		virtual void ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) = 0;

		virtual ~IAuthorityManagerGetters() = default;
	};
	
	/** Responds to FConcertChangeAuthority_Request and tracks what objects and properties clients have authority over. */
	class FAuthorityManager : public FNoncopyable
	{
	public:
		
		using FClientId = FGuid;

		FAuthorityManager(IAuthorityManagerGetters& Getters, TSharedRef<IConcertSession> InSession);
		~FAuthorityManager();

		/**
		 * Checks whether the client that sent the identified object had authority to send it.
		 * @return Whether the server should process the object change.
		 */
		bool IsObjectChangeAllowed(const FReplicatedObjectId& ObjectChange) const;

		/** Notifies this manager that the client has left, which means all their authority is now gone. */
		void OnClientLeft(const FClientId& ClientEndpointId);

	private:

		/** Callbacks required to obtain client info. */
		IAuthorityManagerGetters& Getters;
		/** The session under which this manager operates. */
		TSharedRef<IConcertSession> Session;

		using FStreamId = FGuid;
		struct FClientAuthorityData
		{
			/** Objects the client has authority over. */
			TMap<FStreamId, TSet<FSoftObjectPath>> OwnedObjects;
		};
		TMap<FClientId, FClientAuthorityData> ClientAuthorityData;

		EConcertSessionResponseCode HandleChangeAuthorityRequest(
			const FConcertSessionContext& ConcertSessionContext,
			const FConcertChangeAuthority_Request& Request,
			FConcertChangeAuthority_Response& Response
			);

		/** Whether it is legal for this the client identified by ClientId to take control over the object given the stream the client has registered. */
		bool CanTakeAuthority(const FClientAuthorityData& ClientData, const FClientId& ClientId, const FObjectInStreamID& Object) const;

		/** Finds a stream registered with the client by its ID. */
		const FReplicationStreamDescription* FindClientStreamById(const FClientId& ClientId, const FStreamId& StreamId) const;

		/** Iterates through all clients that are already writing to Object. */
		void ForEachClientWithPotentialConflict(
			const FSoftObjectPath& Object,
			TFunctionRef<EBreakBehavior(const FClientId& ClientId, const FConcertPropertySelection& WrittenProperties)> Callback,
			TArrayView<const FClientId> IgnoredClients = {}
			) const;
	};
}

