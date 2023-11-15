// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientUtils.h"

#include "ConcertMessageData.h"
#include "IConcertClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Widgets/ClientName/SClientName.h"

namespace UE::MultiUserClient::ClientUtils
{
	FString GetClientDisplayName(const IConcertClient& InLocalClientInstance, const FGuid& InClientEndpointId)
	{
		const TSharedPtr<IConcertClientSession> Session = InLocalClientInstance.GetCurrentSession();
		const bool bIsLocalClient = ensure(Session) && Session->GetSessionClientEndpointId() == InClientEndpointId;
		if (bIsLocalClient)
		{
			return ConcertClientSharedSlate::SClientName::GetDisplayText(Session->GetLocalClientInfo(), bIsLocalClient).ToString();
		}

		FConcertSessionClientInfo ClientInfo;
		if (ensure(Session) && Session->FindSessionClient(InClientEndpointId, ClientInfo))
		{
			return ConcertClientSharedSlate::SClientName::GetDisplayText(ClientInfo.ClientInfo, bIsLocalClient).ToString();
		}

		ensureMsgf(false, TEXT("Bad args"));
		return {};
	}
	
	bool GetClientDisplayInfo(const IConcertClient& InLocalClientInstance, const FGuid& InClientEndpointId, FConcertClientInfo& OutClientInfo)
	{
		const TSharedPtr<IConcertClientSession> Session = InLocalClientInstance.GetCurrentSession();
		const bool bIsLocalClient = ensure(Session) && Session->GetSessionClientEndpointId() == InClientEndpointId;
		if (bIsLocalClient)
		{
			OutClientInfo = InLocalClientInstance.GetClientInfo();
			return true;
		}

		FConcertSessionClientInfo ClientInfo;
		if (ensure(Session) && Session->FindSessionClient(InClientEndpointId, ClientInfo))
		{
			OutClientInfo = MoveTemp(ClientInfo.ClientInfo);
			return true;
		}

		return false;
	}
	
	TArray<const FReplicationClient*> GetSortedClientList(const IConcertClient& InLocalClientInstance, const FReplicationClientManager& InReplicationManager)
	{
		TArray<const FReplicationClient*> Result;
		TMap<const FReplicationClient*, FConcertClientInfo> ClientToDisplayInfo;
		for (const TNonNullPtr<FRemoteReplicationClient> RemoteClient : InReplicationManager.GetRemoteClients())
		{
			FConcertClientInfo Info;
			if (GetClientDisplayInfo(InLocalClientInstance, RemoteClient->GetEndpointId(), Info))
			{
				Result.Add(RemoteClient);
				ClientToDisplayInfo.Add(RemoteClient, MoveTemp(Info));
			}
		}

		Result.Sort([&ClientToDisplayInfo](const FReplicationClient& Left, const FReplicationClient& Right)
		{
			const FConcertClientInfo& LeftInfo = ClientToDisplayInfo[&Left];
			const FConcertClientInfo& RightInfo = ClientToDisplayInfo[&Right];
			const FText LeftDisplayName = ConcertClientSharedSlate::SClientName::GetDisplayText(LeftInfo, false);
			const FText RightDisplayName = ConcertClientSharedSlate::SClientName::GetDisplayText(RightInfo, false);
			return LeftDisplayName.ToString() <= RightDisplayName.ToString();
		});
		
		Result.Insert(&InReplicationManager.GetLocalClient(), 0);
		return Result;
	}
}
