// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IConcertSession;
struct FConcertBatchReplicationEvent;
struct FConcertObjectReplicationEvent;
struct FConcertSessionContext;
struct FConcertStreamReplicationEvent;

namespace UE::ConcertSyncCore
{
	class FObjectReplicationCache;
	
	/** Receives replicated object data from all message endpoints and stores it in a FObjectReplicationCache. */
	class CONCERTSYNCCORE_API FObjectReplicationReceiver
	{
	public:

		FObjectReplicationReceiver(TSharedRef<IConcertSession> Session, TSharedRef<FObjectReplicationCache> ReplicationCache);
		virtual ~FObjectReplicationReceiver();

	protected:

		/** Whether the object should be processed. */
		virtual bool ShouldAcceptObject(const FConcertSessionContext& SessionContext, const FConcertStreamReplicationEvent& StreamEvent, const FConcertObjectReplicationEvent& ObjectEvent) const { return true; }

	private:

		/** The session that is being received on. */
		TSharedRef<IConcertSession> Session;
		/** Where received data is stored. */
		TSharedRef<FObjectReplicationCache> ReplicationCache;

		void HandleBatchReplicationEvent(const FConcertSessionContext& SessionContext, const FConcertBatchReplicationEvent& Event);
	};
}
