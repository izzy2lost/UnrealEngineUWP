// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationDataSource.h"
#include "Replication/ReplicationStreamObjectID.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSyncCore
{
	class IReplicationDataSource;
	
	/**
	 * Responsible for prioritizing a list of objects and processing them.
	 * 
	 * Processing is defined by the subclass. Some example implementations:
	 *  - serialize the object and send (client),
	 *  - find loaded object and apply replication data (client)
	 *  - send object data to interested clients (server),
	 */
	class CONCERTSYNCCORE_API FObjectReplicationProcessor
	{
	public:

		explicit FObjectReplicationProcessor(TSharedRef<IReplicationDataSource> DataSource);
		virtual ~FObjectReplicationProcessor() = default;

		/**
		 * Processes all changed objects under the given time budget.
		 * The time budget may be exceeded but we'll try not to and to stay as close to the budget as possible.
		 */
		virtual void ProcessObjects(float TimeBudget);
		
	protected:

		struct FObjectProcessArgs
		{
			/** Info about the object to process */
			FReplicationStreamObjectID ObjectInfo;
		};

		FORCEINLINE IReplicationDataSource& GetDataSource() const { return DataSource.Get(); }
		
		/** Processes the object. */
		virtual void ProcessObject(const FObjectProcessArgs& Args) = 0;

	private:
		
		/** Abstracts where replication data comes from: could be generated (clients) or received (server or client) */
		TSharedRef<IReplicationDataSource> DataSource;
	};
}
