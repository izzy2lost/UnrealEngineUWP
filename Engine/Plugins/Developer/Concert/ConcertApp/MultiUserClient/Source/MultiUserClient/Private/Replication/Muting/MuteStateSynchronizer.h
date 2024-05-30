// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/SoftObjectPath.h"

struct FConcertReplication_ChangeMuteState_Request;
struct FConcertReplication_QueryMuteState_Response;

namespace UE::MultiUserClient
{
	class FMuteStateQueryService;

	DECLARE_MULTICAST_DELEGATE(FOnMuteStateChanged);

	/**
	 * Responsible for answering questions about whether an object is muted.
	 * In the future, this may be extended with reasons (e.g. object Actor.Component is muted because Actor is muted).
	 */
	class FMuteStateSynchronizer : public FNoncopyable
	{
	public:

		FMuteStateSynchronizer(FMuteStateQueryService& InMuteQueryService UE_LIFETIMEBOUND);
		~FMuteStateSynchronizer();

		/** @return Whether ObjectPath is muted. */
		bool IsMuted(const FSoftObjectPath& ObjectPath) const { return MutedObjects.Contains(ObjectPath); }

		/**
		 * Update the mute state after the local application has successfully changed mute state.
		 * FMuteStateQueryService will eventually notify us of the change but this applies it instantaneously.
		 */
		void UpdateStateFromSuccessfulChange(const FConcertReplication_ChangeMuteState_Request& Request);

		/** Broadcasts whent the mute state changes (either because FMuteStateQueryService received updated state or because the local application has successfully made a change request). */
		FOnMuteStateChanged& OnMuteStateChanged() { return OnMuteStateChangedDelegate; }
		
	private:

		/** Updates us with the new mute state from the server regularily. */
		FMuteStateQueryService& MuteQueryService;

		/** All the objects that are muted. */
		TSet<FSoftObjectPath> MutedObjects;

		/** Broadcasts whent the mute state changes (either because FMuteStateQueryService received updated state or because the local application has successfully made a change request). */
		FOnMuteStateChanged OnMuteStateChangedDelegate;

		/** Updates MutedObjects from the mute state on the server. */
		void UpdateFromServerState(const FConcertReplication_QueryMuteState_Response& NewMuteState);
	};
}

