// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamModel.h"

#include "Replication/Client/Online/OnlineClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Selection/ISelectionModel.h"

namespace UE::MultiUserClient::Replication
{
	FMultiStreamModel::FMultiStreamModel(IOnlineClientSelectionModel& InOnlineClientSelectionModel, FOnlineClientManager& InClientManager)
		: OnlineClientSelectionModel(InOnlineClientSelectionModel)
		, ClientManager(InClientManager)
	{
		OnlineClientSelectionModel.OnSelectionChanged().AddRaw(this, &FMultiStreamModel::RebuildStreamsSets);
		RebuildStreamsSets();
	}

	void FMultiStreamModel::ForEachClient(TFunctionRef<EBreakBehavior(const FOnlineClient*)> ProcessClient) const
	{
		for (const FOnlineClient* WritableClient : CachedWritableClients)
		{
			if (ProcessClient(WritableClient) == EBreakBehavior::Break)
			{
				return;
			}
		}
	}

	TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> FMultiStreamModel::GetEditableStreams() const
	{
		TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> Result;
		Algo::Transform(CachedWritableClients, Result, [](const FOnlineClient* Client){ return Client->GetClientEditModel(); });
		return Result;
	}

	void FMultiStreamModel::RebuildStreamsSets()
	{
		// It is not safe to iterate through our cached client array because it may contain stale clients that were just removed.
		ClientManager.ForEachClient([this](FOnlineClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
		
		TSet<const FOnlineClient*> WritableStreams;
		OnlineClientSelectionModel.ForEachItem([this, &WritableStreams](FOnlineClient& Client)
		{
			const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> Stream = Client.GetClientEditModel();
			Client.OnModelChanged().AddRaw(this, &FMultiStreamModel::OnStreamExternallyChanged, Stream.ToWeakPtr());
			
			WritableStreams.Add(&Client);
			return EBreakBehavior::Continue;
		});

		const bool bWritableStayedSame = CachedWritableClients.Num() == WritableStreams.Num() && CachedWritableClients.Includes(WritableStreams);
		if (!bWritableStayedSame)
		{
			CachedWritableClients = MoveTemp(WritableStreams);
			OnStreamSetChangedDelegate.Broadcast();
		}
	}

	void FMultiStreamModel::OnStreamExternallyChanged(TWeakPtr<ConcertSharedSlate::IEditableReplicationStreamModel> ChangedStream)
	{
		if (const TSharedPtr<ConcertSharedSlate::IEditableReplicationStreamModel> ChangedStreamPin = ChangedStream.Pin())
		{
			OnReadOnlyStreamChangedDelegate.Broadcast(ChangedStreamPin.ToSharedRef());
		}
	}
}
