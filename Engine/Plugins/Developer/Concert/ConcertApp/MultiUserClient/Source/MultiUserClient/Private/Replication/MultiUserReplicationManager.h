// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "IConcertSession.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"

class IConcertClientSession;
class IConcertSyncClient;

enum class EConcertConnectionStatus : uint8;

namespace UE::MultiUserClient
{
	/**
	 * Interacts with the replication system on behalf of Multi-User to execute actions specific to Multi-User workflows;
	 * this is opposed to other uses of the replication API, e.g. users using the system in a shipped game.
	 *
	 * This class will
	 *  - be used as a model for the MU control views, such as displaying the streams in the current session (TODO DP UE-193541).
	 *  - implement auto-join behavior in response to joining a concert session (TODO DP UE-193538)
	 *
	 * This class implements the Fence design pattern. All knowledge Multi-User might need should be encapsulated by this class.
	 */
	class FMultiUserReplicationManager : public FGCObject
	{
	public:
		
		FMultiUserReplicationManager(TSharedRef<IConcertSyncClient> InClient);
		virtual ~FMultiUserReplicationManager() override;

		UMultiUserReplicationSessionPreset* GetSessionContent() const { return SessionContent; }
		UMultiUserReplicationClientPreset* GetLocalClientContent() const { return LocalClientContent; }

		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FMultiUserReplicationManager"); }
		//~ End FGCObject Interface

	private:

		/** Client through which the replication bridge is accessed. */
		TSharedRef<IConcertSyncClient> Client;

		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationSessionPreset> SessionContent;
		/** Data for the local client. Also part of SessionContent->ClientPresets. */
		TObjectPtr<UMultiUserReplicationClientPreset> LocalClientContent;
		
		void OnSessionConnectionChanged(IConcertClientSession& ConcertClientSession, EConcertConnectionStatus ConcertConnectionStatus);
		
		/** Joins a replication session. */
		void OnJoinSession(IConcertClientSession& ConcertClientSession);
		/** Leaves the current replication session */
		void OnLeaveSession(IConcertClientSession& ConcertClientSession);
		
		void ClearSessionData();
	};
}

