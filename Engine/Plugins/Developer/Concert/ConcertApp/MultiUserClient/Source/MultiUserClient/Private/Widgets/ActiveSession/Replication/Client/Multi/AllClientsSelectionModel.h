// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientSelectionModel.h"

namespace UE::MultiUserClient::Replication
{
	class FOnlineClientManager;
	
	/** Exposes all clients and detects when clients disconnect. */
	class FAllClientsSelectionModel : public IClientSelectionModel
	{
	public:
		
		FAllClientsSelectionModel(FOnlineClientManager& InClientManager);
		virtual ~FAllClientsSelectionModel() override;

		//~ Begin IClientSelectionModel Interface
		virtual void ForEachSelectedClient(TFunctionRef<EBreakBehavior(FOnlineClient&)> ProcessClient) const override;
		virtual FOnSelectionChanged& OnSelectionChanged() override { return OnSelectionChangedDelegate; }
		//~ End IClientSelectionModel Interface

	private:

		/** Informs us when the list of clients changes */
		FOnlineClientManager& ClientManager;
		
		/** Called when the clients ForEachSelectedClient enumerates has changed. */
		FOnSelectionChanged OnSelectionChangedDelegate;
		
		void OnRemoteClientsChanged();
	};
}
