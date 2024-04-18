// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IConcertSession.h"
#include "Replication/Messages/SyncControl.h"

#include "HAL/Platform.h"
#include "Replication/SyncControlState.h"
#include "Templates/UnrealTemplate.h"

namespace UE::ConcertSyncClient::Replication
{
	/**
	 * Receives network messages that explicitly and implicitly change sync control.
	 * Those network messages are fed into FSyncControlState, which parses the messages.
	 * 
	 * Sync control is the set of objects this client is allowed to replicate.
	 */
	class FLocalSyncControl
		: public FNoncopyable
		, ConcertSyncCore::Replication::FSyncControlState
	{
	public:

		explicit FLocalSyncControl(IConcertSession& InSession UE_LIFETIMEBOUND)
			: Session(InSession)
		{
			Session.RegisterCustomEventHandler<FConcertReplication_ChangeSyncControl>(this, &FLocalSyncControl::OnChangeSyncControl);
		}

		~FLocalSyncControl()
		{
			Session.UnregisterCustomEventHandler<FConcertReplication_ChangeSyncControl>(this);
		}

		using FSyncControlState::IsObjectAllowed;
		using FSyncControlState::Num;
		using FSyncControlState::EnumerateAllowedObjects;
		using FSyncControlState::EnumerateChanges;
		// No using for FSyncControlState::Aggregate because we want to limit usage to below ProcessXChange functions

		/** Stores the given change in this data structure. */
		void ProcessSyncControlChange(const FConcertReplication_ChangeSyncControl& Event)
		{
			OnPreSyncControlChangedDelegate.Broadcast();
			AppendChanges(Event);
			OnPostSyncControlChangedDelegate.Broadcast();
		}

		/** Applies implicit and explicit changes to the client's sync control resulting from a completed authority change request and response. */
		void ProcessAuthorityChange(const FConcertReplication_ChangeAuthority_Request& Request, const FConcertReplication_ChangeAuthority_Response& Response)
		{
			OnPreSyncControlChangedDelegate.Broadcast();
			AppendChanges(Request, Response);
			OnPostSyncControlChangedDelegate.Broadcast();
		}

		/**
		 * Applies implicit changes to the client's sync control resulting from losing authority from objects removed from the stream.
		 * You must validate that the request has also been accepted by the server!
		 */
		void ProcessStreamChange(const FConcertReplication_ChangeStream_Request& Request)
		{
			OnPreSyncControlChangedDelegate.Broadcast();
			AppendChanges(Request);
			OnPostSyncControlChangedDelegate.Broadcast();
		}
		
		DECLARE_MULTICAST_DELEGATE(FSyncControlChanged);
		FSyncControlChanged& OnPreSyncControlChanged() { return OnPreSyncControlChangedDelegate; }
		FSyncControlChanged& OnPostSyncControlChanged() { return OnPostSyncControlChangedDelegate; }

	private:

		/** The session to receive sync control changes on. */
		IConcertSession& Session;
		
		FSyncControlChanged OnPreSyncControlChangedDelegate;
		FSyncControlChanged OnPostSyncControlChangedDelegate;

		void OnChangeSyncControl(const FConcertSessionContext&, const FConcertReplication_ChangeSyncControl& Event) { ProcessSyncControlChange(Event); }
	};
}

