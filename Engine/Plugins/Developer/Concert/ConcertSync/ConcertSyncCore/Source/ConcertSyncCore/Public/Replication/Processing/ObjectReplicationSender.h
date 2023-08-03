// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectReplicationProcessor.h"
#include "Replication/Messages/ConcertReplicationEvents.h"

class IConcertSession;

namespace UE::ConcertSyncCore
{
	/**
	 * Sends object data to a specific endpoint ID.
	 */
	class CONCERTSYNCCORE_API FObjectReplicationSender : public FObjectReplicationProcessor
	{
	public:

		FObjectReplicationSender(const FGuid& TargetEndpointId, TSharedRef<IConcertSession> Session, TSharedRef<IReplicationDataSource> DataSource);
		
		//~ Begin FObjectReplicationProcessor Interface
		virtual void ProcessObjects(float TimeBudget) override;
		//~ End FObjectReplicationProcessor Interface

	protected:

		//~ Begin FObjectReplicationProcessor Interface
		virtual void ProcessObject(const FObjectProcessArgs& Args) override;
		//~ End FObjectReplicationProcessor Interface

	private:

		/** The endpoint data will be sent to */
		const FGuid TargetEndpointId;

		/** The session through which replication messages are sent. */
		TSharedRef<IConcertSession> Session;

		/** This event is filled in ProcessObjects and finally sent to TargetEndpointId. */
		FConcertBatchReplicationEvent EventToSend;
	};
}
