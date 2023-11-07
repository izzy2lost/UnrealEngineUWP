// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientUtils.h"

#include "IConcertClient.h"
#include "Widgets/ClientName/SClientName.h"

namespace UE::MultiUserClient::ClientUtils
{
	FString GetClientDisplayName(const IConcertClient& InLocalClientInstance, const FGuid& InClientToGetName)
	{
		const TSharedPtr<IConcertClientSession> Session = InLocalClientInstance.GetCurrentSession();
		const bool bIsLocalClient = ensure(Session) && Session->GetSessionClientEndpointId() == InClientToGetName;
		if (bIsLocalClient)
		{
			return ConcertClientSharedSlate::SClientName::GetDisplayText(Session->GetLocalClientInfo(), bIsLocalClient).ToString();
		}

		FConcertSessionClientInfo ClientInfo;
		if (ensure(Session) && Session->FindSessionClient(InClientToGetName, ClientInfo))
		{
			return ConcertClientSharedSlate::SClientName::GetDisplayText(ClientInfo.ClientInfo, bIsLocalClient).ToString();
		}

		ensureMsgf(false, TEXT("Bad args"));
		return {};
	}
}
