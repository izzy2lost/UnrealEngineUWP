// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAllClientsView.h"

#include "AllClientsSelectionModel.h"
#include "SMultiClientView.h"
#include "Replication/Client/Online/OnlineClientManager.h"

#include "Algo/Transform.h"
#include "Replication/MultiUserReplicationManager.h"

namespace UE::MultiUserClient::Replication
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

	TSet<const FOnlineClient*> SAllClientsView::GetAllClients() const
	{
		TSet<const FOnlineClient*> Result;
		Algo::Transform(ClientManager->GetRemoteClients(), Result, [](const TNonNullPtr<FRemoteClient>& Client)
		{
			return Client.Get();
		});
		Result.Add(&ClientManager->GetLocalClient());
		return Result;
	}
}
