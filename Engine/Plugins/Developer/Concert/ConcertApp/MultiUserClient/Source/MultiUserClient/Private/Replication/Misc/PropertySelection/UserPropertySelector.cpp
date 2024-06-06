// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserPropertySelector.h"

#include "UserPropertySelectionSource.h"
#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::MultiUserClient
{
	FUserPropertySelector::FUserPropertySelector(FReplicationClientManager& InClientManager)
		: ClientManager(InClientManager)
		, PropertySelection(NewObject<UMultiUserReplicationStream>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional))
		, SelectionEditModel(
			// Transact ClientContentStorage
			ConcertClientSharedSlate::CreateTransactionalStreamModel(
				// Read & write the stream data in ClientContentStorage
				ConcertSharedSlate::CreateBaseStreamModel(
					PropertySelection->MakeReplicationMapGetterAttribute()
					),
				*PropertySelection
				)
			)
		, PropertyProcessor(MakeShared<FUserPropertySelectionSource>(*SelectionEditModel, InClientManager))
	{
		RegisterClient(ClientManager.GetLocalClient());
		ClientManager.OnPostRemoteClientAdded().AddRaw(this, &FUserPropertySelector::OnClientAdded);
		FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FUserPropertySelector::OnObjectTransacted);
	}

	FUserPropertySelector::~FUserPropertySelector()
	{
		ClientManager.OnPostRemoteClientAdded().RemoveAll(this);
		ClientManager.ForEachClient([this](FReplicationClient& Client)
		{
			Client.GetStreamSynchronizer().OnServerStateChanged().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
		FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
	}

	void FUserPropertySelector::AddSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		SelectionEditModel->AddObjects({ Object });
		SelectionEditModel->AddProperties({ Object }, Properties);

		OnPropertySelectionChangedDelegate.Broadcast();
	}

	void FUserPropertySelector::RemoveSelectedProperties(UObject* Object, TConstArrayView<FConcertPropertyChain> Properties)
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

	void FUserPropertySelector::RegisterClient(FReplicationClient& Client)
	{
		IClientStreamSynchronizer& StreamSynchronizer = Client.GetStreamSynchronizer();
		TrackProperties(StreamSynchronizer.GetServerState());
		StreamSynchronizer.OnServerStateChanged().AddRaw(this, &FUserPropertySelector::OnServerStateChanged, Client.GetEndpointId());
	}

	void FUserPropertySelector::OnServerStateChanged(const FGuid ClientId)
	{
		const FReplicationClient* Client = ClientManager.FindClient(ClientId);
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
				AddSelectedProperties(Object, { Property });
			}
		}
	}

	void FUserPropertySelector::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& TransactionObjectEvent)
	{
		if (Object == PropertySelection)
		{
			// Refreshes UI
			OnPropertySelectionChangedDelegate.Broadcast();
		}
	}
}
