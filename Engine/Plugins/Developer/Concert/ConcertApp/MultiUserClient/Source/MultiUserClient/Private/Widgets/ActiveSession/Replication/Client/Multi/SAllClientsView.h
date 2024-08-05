// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Selection/AllOnlineClientsSelectionModel.h"

#include "HAL/Platform.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::MultiUserClient::Replication
{
	class FMultiUserReplicationManager;
	class FOnlineClient;
	class FOnlineClientManager;

	/** Leverages SMultiClientView to display all replication clients. */
	class SAllClientsView : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SAllClientsView){}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedRef<IConcertClient> InConcertClient, FMultiUserReplicationManager& InMultiUserReplicationManager UE_LIFETIMEBOUND);

	private:

		/** Used to get all the replication clients and listen for client changes. */
		FOnlineClientManager* ClientManager = nullptr;

		/** Keeps the SMultiClientView updated of any changes to clients (e.g. disconnects, etc.) */
		TUniquePtr<FAllOnlineClientsSelectionModel> AllClientsModel;

		/** Gets all the clients to display */
		TSet<const FOnlineClient*> GetAllClients() const;
	};
}
