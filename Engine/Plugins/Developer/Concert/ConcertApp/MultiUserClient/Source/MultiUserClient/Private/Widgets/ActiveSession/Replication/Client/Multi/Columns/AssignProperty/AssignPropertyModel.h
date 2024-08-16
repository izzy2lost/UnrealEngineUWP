// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/SoftObjectPtr.h"

struct FConcertPropertyChain;

namespace UE::MultiUserClient::Replication
{
	class FOnlineClient;
	class FOnlineClientManager;
}

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	enum class EPropertyOnObjectsOwnershipState : uint8
	{
		/** Clients owns the property on all objects */
		OwnedOnAllObjects,
		/** does not own the property on any object */
		NotOwnedOnAllObjects,
		
		Mixed
	};

	/** Implements the model in the MVC pattern for the property assignment column. */
	class FAssignPropertyModel : public FNoncopyable
	{
	public:
		
		FAssignPropertyModel(FOnlineClientManager& InClientManager UE_LIFETIMEBOUND);
		~FAssignPropertyModel();

		/** @return Whether property ownership can be changed for the given client. */
		bool CanChangePropertyFor(const FGuid& ClientId, FText* Reason = nullptr) const;
		/** @return Whether it is valid to call ClearProperty with this parameters. */
		bool CanClear(TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property) const;
		/** @return How the property is owned by ClientId */
		EPropertyOnObjectsOwnershipState GetPropertyOwnershipState(const FGuid& ClientId, TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property) const;
		void AssignPropertyTo(const FOnlineClient* Client, TConstArrayView<TSoftObjectPtr<>, signed int> Objects, const FConcertPropertyChain& Property);

		/** Assigns the property to ClientId or unassigns it. Removes the property from all other clients in both cases. */
		void TogglePropertyFor(const FGuid& ClientId, TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property);
		/** Removes the property from all clients. */
		void ClearProperty(TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property) const;
		
		/** Broadcasts when property ownership may have changed. */
		FSimpleMulticastDelegate& OnOwnershipChanged() { return OnOwnershipChangedDelegate; }

	private:

		/** Used to get all clients that can be assigned / reassigned to. */
		FOnlineClientManager& ClientManager;

		/** Broadcasts when property ownership may have changed. */
		FSimpleMulticastDelegate OnOwnershipChangedDelegate;
		
		void BroadcastOnOwnershipChanged() const { OnOwnershipChangedDelegate.Broadcast(); }
	};
}
