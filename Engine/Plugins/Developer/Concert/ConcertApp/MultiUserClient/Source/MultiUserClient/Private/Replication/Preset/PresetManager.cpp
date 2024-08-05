// Copyright Epic Games, Inc. All Rights Reserved.

#include "PresetManager.h"

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "ConcertLogGlobal.h"
#include "IConcertSyncClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Misc/ReplicationStreamUtils.h"
#include "Replication/Muting/MuteStateManager.h"
#include "Replication/Stream/MultiUserStreamId.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

#include "FileHelpers.h"

namespace UE::MultiUserClient::Replication
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

		static void FillStreamAndAuthorityRequest(
			FConcertReplication_PutState_Request& Request,
			const UMultiUserReplicationSessionPreset& Preset,
			const IConcertClientSession& Session,
			bool bClearUnreferencedClients
			)
		{
			const auto AddClient = [&Preset, &Request, bClearUnreferencedClients](const FConcertSessionClientInfo& ClientSessionInfo)
			{
				const UMultiUserReplicationClientContent* ClientSessionContent = Preset.GetClientContent(ClientSessionInfo.ClientInfo);
				if (!ClientSessionContent)
				{
					if (bClearUnreferencedClients)
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
				Stream.BaseDescription.FrequencySettings = ClientSessionContent->Stream->FrequencySettings;
				Request.NewStreams.Add(ClientSessionInfo.ClientEndpointId, { TArray{ Stream } });
				
				// MU automatically requests authority when it adds an object.
				// We'll assume that that authority was granted when the preset was created - if it actually was not, our request may fail due to overlapping authority.
				TArray<FConcertObjectInStreamID>& OwnedObjects = Request.NewAuthorityState.Add(ClientSessionInfo.ClientEndpointId).Objects;
				Algo::Transform(Stream.BaseDescription.ReplicationMap.ReplicatedObjects, OwnedObjects, [&StreamId](const TPair<FSoftObjectPath, FConcertReplicatedObjectInfo>& Pair)
				{
					return FConcertObjectInStreamID{ StreamId, Pair.Key };
				});
			};
			AddClient({ Session.GetSessionClientEndpointId(), Session.GetLocalClientInfo() });
			for (const FConcertSessionClientInfo& ClientSessionInfo : Session.GetSessionClients())
			{
				AddClient(ClientSessionInfo);
			}
		}

		static void FillMuteStateRequest(
			FConcertReplication_PutState_Request& Request,
			const UMultiUserReplicationSessionPreset& Preset,
			const IConcertClientSession& Session
			)
		{
			FConcertReplication_ChangeMuteState_Request& MuteRequest = Request.MuteChange;
			MuteRequest.Flags = EConcertReplicationMuteRequestFlags::ClearMuteState;
			
			const auto IsReferencedByConnectedClient = [&Request, &Session](const FSoftObjectPath& ObjectPath)
			{
				return Algo::AnyOf(Request.NewStreams, [&Session, &ObjectPath](const TPair<FGuid, FConcertReplicationStreamArray>& ClientContent)
				{
					const FGuid& EndpointId = ClientContent.Key;
					FConcertSessionClientInfo Dummy;
					
					const bool bIsConnected = Session.GetSessionClientEndpointId() == EndpointId || Session.FindSessionClient(EndpointId, Dummy);
					// Case: User muted Floor but only Floor.StaticMeshComponent0 is replicated. Hence, also look for child objects being referenced.
					const bool bIsReferenced = ConcertSyncCore::IsObjectOrChildReferenced(ClientContent.Value.Streams, ObjectPath);
					
					return bIsConnected && bIsReferenced;
				});
			};

			const FMultiUserMuteSessionContent& MuteContent = Preset.GetMuteContent();
			for (const TPair<FSoftObjectPath, FConcertReplication_ObjectMuteSetting>& MutedObject : MuteContent.MutedObjects)
			{
				if (IsReferencedByConnectedClient(MutedObject.Key))
				{
					MuteRequest.ObjectsToMute.Add(MutedObject.Key, MutedObject.Value);
				}
			}
			for (const TPair<FSoftObjectPath, FConcertReplication_ObjectMuteSetting>& UnmutedObject : MuteContent.UnmutedObjects)
			{
				if (IsReferencedByConnectedClient(UnmutedObject.Key))
				{
					MuteRequest.ObjectsToUnmute.Add(UnmutedObject.Key, UnmutedObject.Value);
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

			const bool bClearUnreferencedClients = EnumHasAnyFlags(Flags, EApplyPresetFlags::ClearUnreferencedClients);
			FillStreamAndAuthorityRequest(Request, Preset, Session, bClearUnreferencedClients);

			// TODO UE-219829: Once the server allows sending the mute state disconnected clients should have when they rejoin,
			// simply send over all mute state instead of doing filtering here.
			FillMuteStateRequest(Request, Preset, Session);
			
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

		static TArray<TPair<const FOnlineClient*, FConcertClientInfo>> DetermineSavedClients(
			const FOnlineClientManager& ClientManager,
			const IConcertClientSession& Session,
			const FSavePresetOptions& Options
			)
		{
			TArray<TPair<const FOnlineClient*, FConcertClientInfo>> IncludedClients;
			ClientManager.ForEachClient([&Session, &IncludedClients, &Options](const FOnlineClient& Client)
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
	
	FPresetManager::FPresetManager(
		const IConcertSyncClient& SyncClient,
		const FOnlineClientManager& ClientManager,
		const FMuteStateSynchronizer& MuteStateSynchronizer
		)
		: SyncClient(SyncClient)
		, ClientManager(ClientManager)
		, MuteStateSynchronizer(MuteStateSynchronizer)
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
		
		const TArray<TPair<const FOnlineClient*, FConcertClientInfo>> IncludedClients = Private::DetermineSavedClients(ClientManager, *Session, Options);
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

		const TArray<TPair<const FOnlineClient*, FConcertClientInfo>> IncludedClients = Private::DetermineSavedClients(ClientManager, *Session, Options);
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
		for (const TPair<const FOnlineClient*, FConcertClientInfo>& ClientData : IncludedClients)
		{
			const auto[Client, ClientInfo] = ClientData;
			UMultiUserReplicationStream* CopiedClientStream = Client->GetClientStreamObject();
			UMultiUserReplicationClientContent* TargetClientPreset = Preset->AddClientIfUnique(ClientInfo, MultiUserStreamID);
			if (!TargetClientPreset)
			{
				UE_LOG(LogConcert, Warning,
					TEXT("There are multiple clients with display name %s and device name %s in the session. Only the 1st encountered will be saved into the preset. Did you perhaps launch 2 editors on the same machine (if so you can use -CONCERTDISPLAYNAME)?"),
					*ClientInfo.DisplayName,
					*ClientInfo.DeviceName
					);
				continue;
			}

			TargetClientPreset->Stream->Copy(*CopiedClientStream);
			// TODO UE-219834: Once UMultiUserReplicationStream::FrequencySettings reflect the server state, this can be removed.
			TargetClientPreset->Stream->FrequencySettings = Client->GetStreamSynchronizer().GetFrequencySettings();
		}

		Preset->SetMuteContent(
			FMultiUserMuteSessionContent(MuteStateSynchronizer.GetExplicitlyMutedObjects(), MuteStateSynchronizer.GetExplicitlyUnmutedObjects())
			);
		
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
