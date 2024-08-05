// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Assets/MultiUserReplicationStream.h"

#include "Replication/Client/Online/RemoteClient.h"

#include "HAL/Platform.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"

class FReferenceCollector;
class FTransactionObjectEvent;
class UObject;

namespace UE::ConcertSharedSlate
{
	class IEditableReplicationStreamModel;
	class IPropertySourceProcessor;
}

namespace UE::MultiUserClient::Replication
{
	class FUserPropertySelectionSource;
	class FOnlineClientManager;
	
	/**
	 * Manages the properties the user is iterating on in the replication session.
	 * The bottom-half property section in the replication UI uses this to keep track of which properties the user has selected for which properties.
	 *
	 * Whenever any client adds a property to its stream, we'll assume the user is iterating on that property.
	 * For this reason, we automatically will track the property as user-selected.
	 */
	class FUserPropertySelector
		: public FNoncopyable
		, public FGCObject
	{
	public:
		
		FUserPropertySelector(FOnlineClientManager& InClientManager UE_LIFETIMEBOUND);
		~FUserPropertySelector();

		/** Add Properties from the user's selection for Object. */
		void AddSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties);
		/** Removes Properties from the user's selection for Object. */
		void RemoveSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties);
		/** @return Whether Property is selected for Object. */
		bool IsPropertySelected(const FSoftObjectPath& Object, const FConcertPropertyChain& Property) const;

		TSharedRef<ConcertSharedSlate::IPropertySourceProcessor> GetPropertySourceProcessor() const;

		DECLARE_MULTICAST_DELEGATE(FOnPropertySelectionChanged)
		/** @return Event that broadcasts when the user property selection changes. */
		FOnPropertySelectionChanged& OnPropertySelectionChanged() { return OnPropertySelectionChangedDelegate; }
		
		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FUserPropertySelector"); }
		//~ Begin FGCObject Interface

	private:

		/** Used to remove deselected properties from local client's stream. */
		FOnlineClientManager& ClientManager;

		/** This underlying object saves the properties that user has selected. It allows for transactions. */
		TObjectPtr<UMultiUserReplicationStream> PropertySelection;
		/** This logic modifies PropertySelection. */
		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> SelectionEditModel;

		/** Getter for UI to determine which properties to display. */
		const TSharedRef<FUserPropertySelectionSource> PropertyProcessor;

		/** Broadcasts when the user property selection changes. */
		FOnPropertySelectionChanged OnPropertySelectionChangedDelegate;

		/** Called when a remote client joins. */
		void OnClientAdded(FRemoteClient& Client) { RegisterClient(Client); }
		/** Ensures that whenever the client's server state changes, its properties are tracked as user selected. */
		void RegisterClient(FOnlineClient& Client);

		/** Tracks all properties of the client as user selected. */
		void OnServerStateChanged(const FGuid ClientId);
		/** Adds all properties in the replication map as user selected. */
		void TrackProperties(const FConcertObjectReplicationMap& ReplicationMap);

		/** If PropertySelection is transacted, broadcast OnPropertySelectionChangedDelegate. */
		void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent&) const;
		
		void InternalAddSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties);
		void InternalRemoveSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties);
	};
}

