// Copyright Epic Games, Inc. All Rights Reserved.

#include "PresetManager.h"

#include "ConcertLogGlobal.h"
#include "Assets/MultiUserReplicationSessionPreset.h"

#include "FileHelpers.h"
#include "IConcertSyncClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

namespace UE::MultiUserClient
{
	namespace Private
	{
		static void RemoveEmptyObjectsFromRequest(FConcertReplicationStream& Stream)
		{
			TMap<FSoftObjectPath, FConcertReplicatedObjectInfo>& ReplicationMap = Stream.BaseDescription.ReplicationMap.ReplicatedObjects;
			for (auto It = ReplicationMap.CreateIterator(); It; ++It)
			{
				const bool bIsEmpty = It->Value.PropertySelection.ReplicatedProperties.IsEmpty();
				if (bIsEmpty)
				{
					It.RemoveCurrent();
				}
			}
		}
		
		static FConcertReplication_PutState_Request BuildRequest(
			const UMultiUserReplicationSessionPreset& Preset,
			const IConcertClientSession& Session,
			EApplyPresetFlags Flags
			)
		{
			FConcertReplication_PutState_Request Request;
			
			const auto AddClient = [&Preset, &Request, Flags](const FConcertSessionClientInfo& ClientSessionInfo)
			{
				const UMultiUserReplicationClientContent* ClientSessionContent = Preset.GetClientContent(ClientSessionInfo.ClientInfo);
				if (!ClientSessionContent)
				{
					if (EnumHasAnyFlags(Flags, EApplyPresetFlags::ClearUnreferencedClients))
					{
						Request.NewStreams.Add(ClientSessionInfo.ClientEndpointId, {});
					}
					return;
				}

				const UMultiUserReplicationStream* SavedStream = ClientSessionContent->Stream;
				if (SavedStream->ReplicationMap.IsEmpty())
				{
					// This causes the client's content to be cleared.
					Request.NewStreams.Add(ClientSessionInfo.ClientEndpointId, {});
					return;
				}
				
				const FGuid& StreamId = SavedStream->StreamId;
				FConcertReplicationStream Stream { { .Identifier = StreamId, .ReplicationMap = SavedStream->ReplicationMap } };
				// Empty objects will be rejected by the server
				RemoveEmptyObjectsFromRequest(Stream); 
				Request.NewStreams.Add(ClientSessionInfo.ClientEndpointId, { TArray{ Stream } });
				
				// MU automatically requests authority when it adds an object.
				// We'll assume that that authority was granted - if it actually was not, our request may fail due to overlapping authority.
				TArray<FConcertObjectInStreamID>& OwnedObjects = Request.NewAuthorityState.Add(ClientSessionInfo.ClientEndpointId).Objects;
				Algo::Transform(Stream.BaseDescription.ReplicationMap.ReplicatedObjects, OwnedObjects, [&StreamId](const TPair<FSoftObjectPath, FConcertReplicatedObjectInfo>& Pair)
				{
					return FConcertObjectInStreamID{ StreamId, Pair.Key };
				});
				
				// TODO UE-219639: Load frequency settings
			};
			AddClient({ Session.GetSessionClientEndpointId(), Session.GetLocalClientInfo() });
			for (const FConcertSessionClientInfo& ClientSessionInfo : Session.GetSessionClients())
			{
				AddClient(ClientSessionInfo);
			}
			
			// TODO UE-219427: When implementing mute state, you must skip muting objects that may no be referenced due to some client not being present.
			return Request;
		}

		static EReplaceSessionContentErrorCode ExtractErrorCode(const FConcertReplication_PutState_Response& Response)
		{
			switch (Response.ResponseCode)
			{
			case EConcertReplicationPutStateResponseCode::Success: return EReplaceSessionContentErrorCode::Success;
			case EConcertReplicationPutStateResponseCode::Timeout: return EReplaceSessionContentErrorCode::Timeout;
			case EConcertReplicationPutStateResponseCode::FeatureDisabled: return EReplaceSessionContentErrorCode::FeatureDisabled;
							
			case EConcertReplicationPutStateResponseCode::ClientUnknown: [[fallthrough]];
			case EConcertReplicationPutStateResponseCode::StreamError: [[fallthrough]];
			case EConcertReplicationPutStateResponseCode::AuthorityConflict: [[fallthrough]];
			case EConcertReplicationPutStateResponseCode::MuteError: [[fallthrough]];
			default: return EReplaceSessionContentErrorCode::Rejected; 
			}
		}

		static void RemoveEmptyObjectsFromLocalClient(ConcertSharedSlate::IEditableReplicationStreamModel& EditModel)
		{
			TArray<FSoftObjectPath> EmptyObjects;
			EditModel.ForEachReplicatedObject([&EditModel, &EmptyObjects](const FSoftObjectPath& Object)
			{
				if (EditModel.GetNumProperties(Object) == 0)
				{
					EmptyObjects.Add(Object);
				}
				return EBreakBehavior::Continue;
			});
			EditModel.RemoveObjects(EmptyObjects);
		}

		static TArray<TPair<const FReplicationClient*, FConcertClientInfo>> DetermineSavedClients(
			const FReplicationClientManager& ClientManager,
			const IConcertClientSession& Session,
			const FSavePresetOptions& Options
			)
		{
			TArray<TPair<const FReplicationClient*, FConcertClientInfo>> IncludedClients;
			ClientManager.ForEachClient([&Session, &IncludedClients, &Options](const FReplicationClient& Client)
			{
				FConcertClientInfo ClientInfo;
				const bool bGotClientInfo = ClientUtils::GetClientDisplayInfo(Session, Client.GetEndpointId(), ClientInfo);
				if (!ensure(bGotClientInfo))
				{
					return EBreakBehavior::Continue;
				}

				const bool bIsFilteredOut = Options.ClientFilterDelegate.IsBound() && Options.ClientFilterDelegate.Execute(ClientInfo) == EFilterResult::Exclude; 
				if (bIsFilteredOut)
				{
					return EBreakBehavior::Continue;
				}
			
				IncludedClients.Emplace(&Client, ClientInfo);
				return EBreakBehavior::Continue;
			});
			return IncludedClients;
		}
	}
	
	FPresetManager::FPresetManager(const IConcertSyncClient& SyncClient, const FReplicationClientManager& ClientManager)
		: SyncClient(SyncClient)
		, ClientManager(ClientManager)
	{
		IConcertClientReplicationManager& ReplicationManager = *SyncClient.GetReplicationManager();
		ReplicationManager.OnPostRemoteEditApplied().AddRaw(this, &FPresetManager::OnPostRemoteEditApplied);
	}

	FPresetManager::~FPresetManager()
	{
		IConcertClientReplicationManager& ReplicationManager = *SyncClient.GetReplicationManager();
		ReplicationManager.OnPostRemoteEditApplied().RemoveAll(this);
	
		if (InProgressSessionReplacementOp)
		{
			InProgressSessionReplacementOp->EmplaceValue(FReplaceSessionContentResult{ EReplaceSessionContentErrorCode::Cancelled });
			InProgressSessionReplacementOp.Reset();
		}
	}

	TFuture<FReplaceSessionContentResult> FPresetManager::ReplaceSessionContentWithPreset(const UMultiUserReplicationSessionPreset& Preset, EApplyPresetFlags Flags)
	{
		if (!ensure(!IsPresetChangeInProgress()))
		{
			return MakeFulfilledPromise<FReplaceSessionContentResult>(EReplaceSessionContentErrorCode::InProgress).GetFuture();
		}
		
		const TSharedPtr<IConcertClientSession> Session = SyncClient.GetConcertClient()->GetCurrentSession();
		if (!ensure(Session))
		{
			return MakeFulfilledPromise<FReplaceSessionContentResult>(EReplaceSessionContentErrorCode::Timeout).GetFuture();
		}
		
		InProgressSessionReplacementOp = MakeShared<TPromise<FReplaceSessionContentResult>>();
		SyncClient.GetReplicationManager()
			->PutClientState(Private::BuildRequest(Preset, *Session, Flags))
			.Next(
				[this, WeakPromise = TWeakPtr<TPromise<FReplaceSessionContentResult>>(InProgressSessionReplacementOp)]
				(FConcertReplication_PutState_Response&& Response)
			{
				// If WeakPromise is stale, the request completes after our owning FPresetManager has been destroyed.
				// In that case, it is not safe to access this.
				if (const TSharedPtr<TPromise<FReplaceSessionContentResult>> PromisePin = WeakPromise.Pin())
				{
					// Destroy before emplacing promise because its future may trigger another ReplaceSessionContentWithPreset call (unlikely); PromisePin will keep it alive for now.
					InProgressSessionReplacementOp.Reset();
					PromisePin->EmplaceValue(Private::ExtractErrorCode(Response));

					// The client may have added an object via the Add button but not assigned any properties.
					// Those empty objects exist locally only and were never submitted to the server.
					// Remove those because it is a client expectation that the final list only contains the objects that were in the preset.
					ConcertSharedSlate::IEditableReplicationStreamModel& EditModel = *ClientManager.GetLocalClient().GetClientEditModel();
					Private::RemoveEmptyObjectsFromLocalClient(EditModel);
				}
			});
		
		return InProgressSessionReplacementOp->GetFuture();
	}

	ECanSaveResult FPresetManager::CanSavePreset(const FSavePresetOptions& Options) const
	{
		const TSharedPtr<IConcertClientSession> Session = SyncClient.GetConcertClient()->GetCurrentSession();
		checkf(Session, TEXT("FPresetManager is only supposed to exist while in a session"));
		
		const TArray<TPair<const FReplicationClient*, FConcertClientInfo>> IncludedClients = Private::DetermineSavedClients(ClientManager, *Session, Options);
		return IncludedClients.IsEmpty()
			? ECanSaveResult::NoClients
			: ECanSaveResult::Yes;
	}

	UMultiUserReplicationSessionPreset* FPresetManager::ExportToPresetAndSaveAs(const FSavePresetOptions& Options)
	{
		UMultiUserReplicationSessionPreset* Preset = ExportToPreset(Options);
		if (Preset)
		{
			TArray<UObject*> SavedAssets;
			FEditorFileUtils::SaveAssetsAs({ Preset }, SavedAssets);
		}
		return Preset;
	}

	UMultiUserReplicationSessionPreset* FPresetManager::ExportToPreset(const FSavePresetOptions& Options) const
	{
		const TSharedPtr<IConcertClientSession> Session = SyncClient.GetConcertClient()->GetCurrentSession();
		checkf(Session, TEXT("FPresetManager is only supposed to exist while in a session"));

		const TArray<TPair<const FReplicationClient*, FConcertClientInfo>> IncludedClients = Private::DetermineSavedClients(ClientManager, *Session, Options);
		if (IncludedClients.IsEmpty())
		{
			return nullptr;
		}
		
		UMultiUserReplicationSessionPreset* Preset = NewObject<UMultiUserReplicationSessionPreset>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UMultiUserReplicationSessionPreset::StaticClass(), TEXT("ReplicationPreset")),
			// Mark as transient so FEditorFileUtils::SaveAssetsAs creates a new package for the object.
			RF_Transient
			);
		for (const TPair<const FReplicationClient*, FConcertClientInfo>& ClientData : IncludedClients)
		{
			const auto[Client, ClientInfo] = ClientData;
			UMultiUserReplicationClientContent* ClientContent_ToCopy = Client->GetClientContent();
			UMultiUserReplicationClientContent* ClientContent_InPreset = Preset->AddClientIfUnique(ClientInfo);
			if (!ClientContent_InPreset)
			{
				UE_LOG(LogConcert, Warning,
					TEXT("There are multiple clients with display name %s and device name %s in the session. Only the 1st encountered will be saved into the preset. Did you perhaps launch 2 editors on the same machine (if so you can use -CONCERTDISPLAYNAME)?"),
					*ClientInfo.DisplayName,
					*ClientInfo.DeviceName
					);
				continue;
			}

			ClientContent_InPreset->Stream->Copy(*ClientContent_ToCopy->Stream);
		}
		
		return Preset;
	}

	void FPresetManager::OnPostRemoteEditApplied(const ConcertSyncClient::Replication::FRemoteEditEvent& Event) const
	{
		if (Event.Reason == EConcertReplicationChangeClientReason::PutRequest)
		{
			// The client may have added an object via the Add button but not assigned any properties.
			// Those empty objects exist locally only and were never submitted to the server.
			// Remove those because it is a client expectation that the final list only contains the objects that were in the preset.
			ConcertSharedSlate::IEditableReplicationStreamModel& EditModel = *ClientManager.GetLocalClient().GetClientEditModel();
			Private::RemoveEmptyObjectsFromLocalClient(EditModel);
		}
	}
}
