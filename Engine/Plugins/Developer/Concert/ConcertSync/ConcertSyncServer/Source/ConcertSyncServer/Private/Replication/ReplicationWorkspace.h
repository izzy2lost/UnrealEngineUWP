// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IReplicationWorkspace.h"

#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"

class FConcertSyncSessionDatabase;

namespace UE::ConcertSyncServer
{
	DECLARE_DELEGATE_RetVal_OneParam(TOptional<FConcertSessionClientInfo>, FFindSessionClient, const FGuid& EndpointId);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FShouldIgnoreClientActivityOnRestore, const FGuid& ClientId);
	
	/**
	 * Implements the replication workspace server-side.
	 * 
	 * At runtime, this is created by FConcertServerWorkspace.
	 * This exists as independent class so it can be unit-tested.
	 */
	class FReplicationWorkspace : public Replication::IReplicationWorkspace
	{
	public:

		FReplicationWorkspace(
			FConcertSyncSessionDatabase& Database UE_LIFETIMEBOUND,
			FFindSessionClient InFindSessionClientDelegate,
			FShouldIgnoreClientActivityOnRestore InShouldIgnoreClientActivityOnRestoreDelegate
			);

		/** Called when an activity is added to the database. */
		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAddReplicationActivity, int64 ActivityId, bool bSuccess);
		FOnAddReplicationActivity& OnAddReplicationActivity() { return OnAddReplicationActivityDelegate; }

		//~ Begin IReplicationWorkspace Interface
		virtual TOptional<int64> ProduceClientLeaveReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_LeaveReplication& EventData) override;
		virtual TOptional<int64> ProduceClientMuteReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_Mute& EventData) override;
		virtual bool GetLastLeaveReplicationActivityByClient(const FConcertSessionClientInfo& InClientInfo, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const override;
		virtual bool GetLeaveReplicationActivityById(const int64 ActivityId, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const override;
		virtual void EnumerateMuteActivities(TFunctionRef<EBreakBehavior(const FConcertSyncReplicationActivity& Activity)> Callback) const override;
		//~ End IReplicationWorkspace Interface

	private:

		/** Needed to get replication data. */
		FConcertSyncSessionDatabase& Database;
		/** Needed by GetLastLeaveReplicationActivityByClient to get the most appropriate activity data. */
		const FFindSessionClient FindSessionClientDelegate;
		/** Needed by ProduceClientLeaveReplicationActivity to correctly build the activity data. */
		const FShouldIgnoreClientActivityOnRestore ShouldIgnoreClientActivityOnRestoreDelegate;
		
		FOnAddReplicationActivity OnAddReplicationActivityDelegate;

		template<typename TPayload>
		TOptional<int64> ProduceActivity(const FGuid& EndpointId, const TPayload& EventData);
	};
}

