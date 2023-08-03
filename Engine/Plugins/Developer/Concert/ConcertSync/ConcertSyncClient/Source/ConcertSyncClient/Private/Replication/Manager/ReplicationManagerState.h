// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"

namespace UE::ConcertSyncClient::Replication
{
	class FReplicationManager;
	
	/**
	 * Implements the State design pattern (see GOF) for FConcertClientReplicationManager.
	 * Depending on the handshake state, the replication manager will react differently to the implemented IConcertClientReplicationManager functions.
	 */
	class FReplicationManagerState : public IConcertClientReplicationManager, public TSharedFromThis<FReplicationManagerState>
	{
	public:

		FReplicationManagerState(FReplicationManager& Owner);

	protected:

		/**
		 * Subclasses can change the state with this function.
		 * Important: This decrements the reference count so it may destroy your state unless you keep it alive manually!
		 */
		void ChangeState(TSharedRef<FReplicationManagerState> NewState);
		
		const FReplicationManager& GetOwner() const { return Owner; }
		FReplicationManager& GetOwner() { return Owner; }
		
	private:

		/** Used to change the state on the owning manager. */
		FReplicationManager& Owner;
		
		/**
		 * Do any logic for entering state here instead of constructor.
		 * 
		 * Important to handle "recursive" calls to ChangState and also constructor does not have access to SharedThis.
		 */
		virtual void OnEnterState() {}
	};

}
