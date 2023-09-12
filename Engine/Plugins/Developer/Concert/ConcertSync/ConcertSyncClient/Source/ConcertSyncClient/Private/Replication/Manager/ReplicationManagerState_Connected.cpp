// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManagerState_Connected.h"

#include "ConcertLogGlobal.h"
#include "ConcertTransportMessages.h"
#include "IConcertSession.h"
#include "ReplicationManagerState_Disconnected.h"
#include "Replication/Formats/FullObjectFormat.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/Processing/ClientReplicationDataCollector.h"
#include "Replication/Processing/ObjectReplicationApplierProcessor.h"
#include "Replication/Processing/ObjectReplicationReceiver.h"
#include "Replication/Processing/ObjectReplicationSender.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::ConcertSyncClient::Replication
{
	FReplicationManagerState_Connected::FReplicationManagerState_Connected(
		TSharedRef<IConcertClientSession> LiveSession,
		IConcertClientReplicationBridge* ReplicationBridge,
		TArray<FReplicationStreamDescription> StreamDescriptions,
		FReplicationManager& Owner
		)
		: FReplicationManagerState(Owner)
		, LiveSession(LiveSession)
		, ReplicationBridge(ReplicationBridge)
		, RegisteredStreams(MoveTemp(StreamDescriptions))
		// TODO: Use config to determine which replication format to use
		, ReplicationFormat(MakeShared<ConcertSyncCore::FFullObjectFormat>())
		, ReplicationDataSource(MakeShared<FClientReplicationDataCollector>(ReplicationBridge, ReplicationFormat, RegisteredStreams))
		, Sender(MakeShared<ConcertSyncCore::FObjectReplicationSender>(LiveSession->GetSessionServerEndpointId(), LiveSession, ReplicationDataSource))
		, ReceivedDataCache(MakeShared<ConcertSyncCore::FObjectReplicationCache>(ReplicationFormat))
		, Receiver(MakeShared<ConcertSyncCore::FObjectReplicationReceiver>(LiveSession, ReceivedDataCache))
		, ReceivedReplicationQueuer(FClientReplicationDataQueuer::Make(ReplicationBridge, ReceivedDataCache))
		, ReplicationApplier(MakeShared<FObjectReplicationApplierProcessor>(ReplicationBridge, ReplicationFormat, ReceivedReplicationQueuer))
	{}

	FReplicationManagerState_Connected::~FReplicationManagerState_Connected()
	{
		// Technically not needed due to AddSP but let's be nice and clean up after ourselves
		LiveSession->OnTick().RemoveAll(this);
	}

	TFuture<FJoinReplicatedSessionResult> FReplicationManagerState_Connected::JoinReplicationSession(FJoinReplicatedSessionArgs Args)
	{
		UE_LOG(LogConcert, Warning, TEXT("JoinReplicationSession requested while already in a session"));
		return MakeFulfilledPromise<FJoinReplicatedSessionResult>(FJoinReplicatedSessionResult{ EJoinReplicationErrorCode::AlreadyInSession }).GetFuture();
	}

	void FReplicationManagerState_Connected::LeaveReplicationSession()
	{
		LiveSession->SendCustomEvent(FConcertReplication_LeaveEvent{}, LiveSession->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		ChangeState(MakeShared<FReplicationManagerState_Disconnected>(LiveSession, ReplicationBridge, GetOwner()));
	}

	IConcertClientReplicationManager::EStreamEnumerationResult FReplicationManagerState_Connected::ForEachRegisteredStream(
		TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback
		) const
	{
		for (const FReplicationStreamDescription& Stream : RegisteredStreams)
		{
			if (Callback(Stream) == EBreakBehavior::Break)
			{
				break;
			}
		}
		return EStreamEnumerationResult::Iterated;
	}

	TFuture<FAuthorityChangeResponse> FReplicationManagerState_Connected::RequestAuthorityChange(FAuthorityChangeRequest Args)
	{
		return LiveSession->SendCustomRequest<FConcertChangeAuthority_Request, FConcertChangeAuthority_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([](FConcertChangeAuthority_Response&& Response)
			{
				return FAuthorityChangeResponse { MoveTemp(Response) };
			});
	}

	TFuture<FClientQueryResponse> FReplicationManagerState_Connected::QueryClientInfo(FClientQueryRequest Args)
	{
		if (EnumHasAllFlags(Args.QueryFlags, EConcertQueryClientStreamFlags::SkipAuthority | EConcertQueryClientStreamFlags::SkipStreamInfo))
		{
			UE_LOG(LogConcert, Warning, TEXT("Request QueryClientInfo is pointless because SkipAuthority and SkipStreamInfo are both set. Returning immediately..."));
			return MakeFulfilledPromise<FClientQueryResponse>().GetFuture();
		}
		
		return LiveSession->SendCustomRequest<FConcertQueryReplicationInfo_Request, FConcertQueryReplicationInfo_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([](FConcertQueryReplicationInfo_Response&& Response)
			{
				return FClientQueryResponse { MoveTemp(Response) };
			});
	}

	void FReplicationManagerState_Connected::OnEnterState()
	{
		LiveSession->OnTick().AddSP(this, &FReplicationManagerState_Connected::Tick);
	}

	void FReplicationManagerState_Connected::Tick(IConcertClientSession& Session, float DeltaTime)
	{
		// TODO: Set this up in a config file
		constexpr double TimeBudget = 1.0 / 60.0;
		double TimeLeft = TimeBudget;

		auto NextIndex = [this](int32 Current){ return AlternatingTickTasks.IsValidIndex(Current + 1) ? Current + 1 : 0; };
		int32 CurrentIndex = NextTickTaskIndex;
		const double StartTime = FPlatformTime::Seconds();
		do
		{
			FTickTask Task = AlternatingTickTasks[CurrentIndex]; 
			Invoke(Task, this, TimeLeft);

			const double TotalElapsedTime = FPlatformTime::Seconds() - StartTime;
			TimeLeft = TimeBudget - TotalElapsedTime;
			CurrentIndex = NextIndex(CurrentIndex);
		}
		while (TimeLeft > 0.0 && CurrentIndex != NextTickTaskIndex);
		NextTickTaskIndex = CurrentIndex;
	}

	void FReplicationManagerState_Connected::TickSender(float TimeBudget)
	{
		Sender->ProcessObjects(TimeBudget);
	}

	void FReplicationManagerState_Connected::TickReceiver(float TimeBudget)
	{
		ReplicationApplier->ProcessObjects(TimeBudget);
	}
}

