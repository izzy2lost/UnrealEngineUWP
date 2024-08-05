// Copyright Epic Games, Inc. All Rights Reserved.

#include "AllClientsSelectionModel.h"

#include "Replication/Client/Online/OnlineClientManager.h"

namespace UE::MultiUserClient::Replication
{
	FAllClientsSelectionModel::FAllClientsSelectionModel(FOnlineClientManager& InClientManager)
		: ClientManager(InClientManager)
	{
		ClientManager.OnRemoteClientsChanged().AddRaw(this, &FAllClientsSelectionModel::OnRemoteClientsChanged);
	}

	FAllClientsSelectionModel::~FAllClientsSelectionModel()
	{
		ClientManager.OnRemoteClientsChanged().RemoveAll(this);
	}

	void FAllClientsSelectionModel::ForEachItem(TFunctionRef<EBreakBehavior(FOnlineClient&)> ProcessClient) const
	{
		ClientManager.ForEachClient(ProcessClient);
	}

	void FAllClientsSelectionModel::OnRemoteClientsChanged()
	{
		OnSelectionChangedDelegate.Broadcast();
	}
}
