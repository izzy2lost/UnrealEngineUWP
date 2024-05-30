// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAllClientsView.h"

#include "AllClientsSelectionModel.h"
#include "SMultiClientView.h"
#include "Replication/Client/ReplicationClientManager.h"

#include "Algo/Transform.h"
#include "Replication/MultiUserReplicationManager.h"

namespace UE::MultiUserClient
{
	void SAllClientsView::Construct(const FArguments&, TSharedRef<IConcertClient> InConcertClient, FMultiUserReplicationManager& InMultiUserReplicationManager)
	{
		ClientManager = InMultiUserReplicationManager.GetClientManager();
		check(ClientManager);
		AllClientsModel = MakeUnique<FAllClientsSelectionModel>(*ClientManager);
		
		ChildSlot
		[
			SNew(SMultiClientView, InConcertClient, InMultiUserReplicationManager, *AllClientsModel)
		];
	}

	TSet<const FReplicationClient*> SAllClientsView::GetAllClients() const
	{
		TSet<const FReplicationClient*> Result;
		Algo::Transform(ClientManager->GetRemoteClients(), Result, [](const TNonNullPtr<FRemoteReplicationClient>& Client)
		{
			return Client.Get();
		});
		Result.Add(&ClientManager->GetLocalClient());
		return Result;
	}
}
