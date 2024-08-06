// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserPropertySelector.h"

#include "UserPropertySelectionSource.h"
#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"

#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "FUserPropertySelector"

namespace UE::MultiUserClient::Replication
{
	FUserPropertySelector::FUserPropertySelector(FOnlineClientManager& InClientManager)
		: ClientManager(InClientManager)
		, PropertySelection(NewObject<UMultiUserReplicationStream>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional))
		, SelectionEditModel(ConcertSharedSlate::CreateBaseStreamModel(PropertySelection->MakeReplicationMapGetterAttribute()))
		, PropertyProcessor(MakeShared<FUserPropertySelectionSource>(*SelectionEditModel, InClientManager))
	{
		RegisterClient(ClientManager.GetLocalClient());
		ClientManager.OnPostRemoteClientAdded().AddRaw(this, &FUserPropertySelector::OnClientAdded);
		FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FUserPropertySelector::OnObjectTransacted);
	}

	FUserPropertySelector::~FUserPropertySelector()
	{
		ClientManager.OnPostRemoteClientAdded().RemoveAll(this);
		ClientManager.ForEachClient([this](FOnlineClient& Client)
		{
			Client.GetStreamSynchronizer().OnServerStreamChanged().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
		FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
	}

	void FUserPropertySelector::AddSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddSelectedProperties", "Select replicated property"));
		PropertySelection->Modify();
		
		InternalAddSelectedProperties(Object, Properties);
	}
	
	void FUserPropertySelector::RemoveSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveSelectedProperties", "Deselect replicated property"));
		PropertySelection->Modify();
		
		InternalRemoveSelectedProperties(Object, Properties);
	}

	bool FUserPropertySelector::IsPropertySelected(const FSoftObjectPath& Object, const FConcertPropertyChain& Property) const
	{
		return ClientManager.GetAuthorityCache().IsPropertyReferencedByAnyClientStream(Object, Property)
			|| SelectionEditModel->HasProperty(Object, Property);
	}

	TSharedRef<ConcertSharedSlate::IPropertySourceProcessor> FUserPropertySelector::GetPropertySourceProcessor() const
	{
		return PropertyProcessor;
	}

	void FUserPropertySelector::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(PropertySelection);
	}

	void FUserPropertySelector::RegisterClient(FOnlineClient& Client)
	{
		IClientStreamSynchronizer& StreamSynchronizer = Client.GetStreamSynchronizer();
		TrackProperties(StreamSynchronizer.GetServerState());
		StreamSynchronizer.OnServerStreamChanged().AddRaw(this, &FUserPropertySelector::OnServerStateChanged, Client.GetEndpointId());
	}

	void FUserPropertySelector::OnServerStateChanged(const FGuid ClientId)
	{
		const FOnlineClient* Client = ClientManager.FindClient(ClientId);
		if (ensure(Client))
		{
			TrackProperties(Client->GetStreamSynchronizer().GetServerState());
		}
	}

	void FUserPropertySelector::TrackProperties(const FConcertObjectReplicationMap& ReplicationMap)
	{
		for (const TPair<FSoftObjectPath, FConcertReplicatedObjectInfo>& Pair : ReplicationMap.ReplicatedObjects)
		{
			UObject* Object = Pair.Key.ResolveObject();
			// The object may come from a remote client that is in a different world than the local application
			if (!Object)
			{
				continue;
			}

			for (const FConcertPropertyChain& Property : Pair.Value.PropertySelection.ReplicatedProperties)
			{
				// Do not transact this change: the user did not actively add these properties, so it should not show up in the undo history.
				InternalAddSelectedProperties(Object, { Property });
			}
		}
	}

	void FUserPropertySelector::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent&) const
	{
		if (Object == PropertySelection)
		{
			// Refreshes UI
			OnPropertySelectionChangedDelegate.Broadcast();
		}
	}
	
	void FUserPropertySelector::InternalAddSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		SelectionEditModel->AddObjects({ Object });
		SelectionEditModel->AddProperties({ Object }, Properties);

		OnPropertySelectionChangedDelegate.Broadcast();
	}
	
	void FUserPropertySelector::InternalRemoveSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		SelectionEditModel->RemoveProperties({ Object }, Properties);
		if (!SelectionEditModel->HasAnyPropertyAssigned(Object))
		{
			SelectionEditModel->RemoveObjects({ Object });
		}

		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> ClientEditModel = ClientManager.GetLocalClient().GetClientEditModel();
		ClientEditModel->RemoveProperties({ Object }, Properties);
		
		OnPropertySelectionChangedDelegate.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE