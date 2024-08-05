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
		for (const FOnlineClient* ReadOnlyClient : GetCachedReadOnlyClients())
		{
			if (ProcessClient(ReadOnlyClient) == EBreakBehavior::Break)
			{
				return;
			}
		}
		for (const FOnlineClient* WritableClient : GetCachedWritableClients())
		{
			if (ProcessClient(WritableClient) == EBreakBehavior::Break)
			{
				return;
			}
		}
	}

	TSet<TSharedRef<ConcertSharedSlate::IReplicationStreamModel>> FMultiStreamModel::GetReadOnlyStreams() const
	{
		TSet<TSharedRef<ConcertSharedSlate::IReplicationStreamModel>> Result;
		Algo::Transform(CachedReadOnlyClients, Result, [](const FOnlineClient* Client){ return Client->GetClientEditModel(); });
		return Result;
	}

	TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> FMultiStreamModel::GetEditableStreams() const
	{
		TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> Result;
		Algo::Transform(CachedWritableClients, Result, [](const FOnlineClient* Client){ return Client->GetClientEditModel(); });
		return Result;
	}

	void FMultiStreamModel::RebuildStreamsSets()
	{
		// Cannot just iterate through CachedReadOnlyClients because it may contain stale clients that were just removed
		ClientManager.ForEachClient([this](FOnlineClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
		
		TSet<const FOnlineClient*> ReadOnlyStreams;
		TSet<const FOnlineClient*> WritableStreams;
		OnlineClientSelectionModel.ForEachItem([this, &ReadOnlyStreams, &WritableStreams](FOnlineClient& Client)
		{
			const bool bIsUploadable = CanEverSubmit(Client.GetSubmissionWorkflow().GetUploadability());
			const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> Stream = Client.GetClientEditModel();
			Client.OnModelChanged().AddRaw(this, &FMultiStreamModel::OnStreamExternallyChanged, Stream.ToWeakPtr());
			
			TSet<const FOnlineClient*>& StreamToAddTo = Client.AllowsEditing() ? WritableStreams : ReadOnlyStreams;
			StreamToAddTo.Add(&Client);
			return EBreakBehavior::Continue;
		});

		const bool bReadOnlyStayedSame = CachedReadOnlyClients.Num() == ReadOnlyStreams.Num() && CachedReadOnlyClients.Includes(ReadOnlyStreams);
		if (!bReadOnlyStayedSame)
		{
			CachedReadOnlyClients = MoveTemp(ReadOnlyStreams);
		}
		const bool bWritableStayedSame = CachedWritableClients.Num() == WritableStreams.Num() && CachedWritableClients.Includes(WritableStreams);
		if (!bWritableStayedSame)
		{
			CachedWritableClients = MoveTemp(WritableStreams);
		}

		if (!bReadOnlyStayedSame || !bWritableStayedSame)
		{
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
