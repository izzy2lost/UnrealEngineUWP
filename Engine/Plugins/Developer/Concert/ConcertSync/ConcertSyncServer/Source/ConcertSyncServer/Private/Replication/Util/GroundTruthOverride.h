// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/AuthorityConflictSharedUtils.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/Platform.h"
#include "Misc/Guid.h"
#include "Templates/UnrealTemplate.h"

struct FConcertReplicationStreamArray;
struct FConcertObjectInStreamArray;

namespace UE::ConcertSyncServer::Replication
{
	class FAuthorityManager;
	class FConcertReplicationClient;
	class IRegistrationEnumerator;
	
	/**
	 * Pretends that the ground truth is the client overrides it was given.
	 * If a setting is not overriden, then it defaults back to the server state.
	 */
	class FGroundTruthOverride
		: public ConcertSyncCore::Replication::AuthorityConflictUtils::IReplicationGroundTruth
		, public FNoncopyable
	{
	public:
		
		FGroundTruthOverride(
			const TMap<FGuid, FConcertReplicationStreamArray>& StreamOverrides UE_LIFETIMEBOUND,
			const TMap<FGuid, FConcertObjectInStreamArray>& AuthorityOverrides UE_LIFETIMEBOUND,
			const IRegistrationEnumerator& Clients UE_LIFETIMEBOUND,
			const FAuthorityManager& AuthorityManager UE_LIFETIMEBOUND
			)
			: StreamOverrides(StreamOverrides)
			, AuthorityOverrides(AuthorityOverrides)
			, Clients(Clients)
			, AuthorityManager(AuthorityManager)
		{
		}

		//~ Begin IReplicationGroundTruth Interface
		virtual void ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FGuid& StreamId, const FConcertObjectReplicationMap& ReplicationMap)> Callback) const override;
		virtual void ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const override;
		virtual bool HasAuthority(const FGuid& ClientId, const FGuid& StreamId, const FSoftObjectPath& ObjectPath) const override;
		//~ Begin IReplicationGroundTruth Interface

	private:

		const TMap<FGuid, FConcertReplicationStreamArray>& StreamOverrides;
		const TMap<FGuid, FConcertObjectInStreamArray>& AuthorityOverrides;

		const IRegistrationEnumerator& Clients;
		const FAuthorityManager& AuthorityManager;
	};
}


