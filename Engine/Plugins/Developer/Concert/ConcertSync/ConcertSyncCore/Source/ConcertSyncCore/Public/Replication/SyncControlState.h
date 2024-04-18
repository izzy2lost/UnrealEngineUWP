// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/ObjectIds.h"
#include "Messages/SyncControl.h"
#include "Misc/EBreakBehavior.h"
#include "Replication/Messages/ChangeAuthority.h"
#include "Replication/Messages/ChangeStream.h"
#include "Replication/Messages/SyncControl.h"

#include "Templates/Function.h"

#include <type_traits>

namespace UE::ConcertSyncCore::Replication
{
	template<typename T>
	concept CObjectInStreamInvocable = std::is_invocable_v<T, const FConcertObjectInStreamID&>;
	
	/**
	 * Keeps track of the sync control (AllowedObjects), which is a set of objects a client is allowed to replicate.
	 * It knows how to parse network messages that explicitly and implicitly change sync control and update AllowedObjects.
	 * 
	 * This class does not know where the network messages come from and is designed to be used as utility by lower level systems.
	 */
	class FSyncControlState
	{
	public:

		FSyncControlState() = default;
		
		// Not explicit to make it useful to use
		FSyncControlState(TSet<FConcertObjectInStreamID>&& AllowedObjects) 
			: AllowedObjects(MoveTemp(AllowedObjects))
		{}
		FSyncControlState(const TSet<FConcertObjectInStreamID>& AllowedObjects)
			: AllowedObjects(AllowedObjects)
		{}

		/** @return Whether Object is allowed to be replicated. */
		bool IsObjectAllowed(const FConcertObjectInStreamID& Object) const { return AllowedObjects.Contains(Object); }
		/** @return Number of allowed objects */
		uint32 Num() const { return AllowedObjects.Num(); }

		/** Enumerates all sync controlled objects. */
		template<typename TProcessObject>
		bool EnumerateAllowedObjects(TProcessObject Callback) const requires std::is_invocable_r_v<EBreakBehavior, TProcessObject, const FConcertObjectInStreamID&>;

		/** Enumerates the changes that would be made by applying this event. */
		template<CObjectInStreamInvocable TOnAllowed, CObjectInStreamInvocable TOnDisallowed>
		static void EnumerateChanges(const FConcertReplication_ChangeSyncControl& Event, TOnAllowed&& OnAllowed, TOnDisallowed&& OnDisallowed);
		
		/** Combines past state with an incoming sync control event. */
		template<CObjectInStreamInvocable TOnAllowed, CObjectInStreamInvocable TOnDisallowed>
		void AppendChanges(const FConcertReplication_ChangeSyncControl& Event, TOnAllowed&& OnAllowed, TOnDisallowed&& OnDisallowed);
		void AppendChanges(const FConcertReplication_ChangeSyncControl& Event);

		/** Combines implicit and explicit sync control changes caused by authority changes. */
		template<CObjectInStreamInvocable TOnAllowed, CObjectInStreamInvocable TOnDisallowed>
		void AppendChanges(const FConcertReplication_ChangeAuthority_Request& Request, const FConcertReplication_ChangeAuthority_Response& Response, TOnAllowed&& OnAllowed, TOnDisallowed&& OnDisallowed);
		void AppendChanges(const FConcertReplication_ChangeAuthority_Request& Request, const FConcertReplication_ChangeAuthority_Response& Response);

		/** Combines implicit sync control changes caused by stream change. */
		template<CObjectInStreamInvocable TOnDisallowed>
		void AppendChanges(const FConcertReplication_ChangeStream_Request& Request, TOnDisallowed&& OnDisallowed);
		void AppendChanges(const FConcertReplication_ChangeStream_Request& Request);
		
		friend bool operator==(const FSyncControlState& Left, const FSyncControlState& Right)
		{
			return Left.AllowedObjects.Num() == Right.AllowedObjects.Num() && Left.AllowedObjects.Includes(Right.AllowedObjects);
		}
		friend bool operator!=(const FSyncControlState& Left, const FSyncControlState& Right) { return !(Left == Right);}

	private:

		/** The objects the client has sync control over, i.e. is allowed to replicate. */
		TSet<FConcertObjectInStreamID> AllowedObjects;
	};
	
	template<typename TProcessObject>
	bool FSyncControlState::EnumerateAllowedObjects(TProcessObject Callback) const requires std::is_invocable_r_v<EBreakBehavior, TProcessObject, const FConcertObjectInStreamID&>
	{
		for (const FConcertObjectInStreamID& Object : AllowedObjects)
		{
			if (Callback(Object) == EBreakBehavior::Break)
			{
				break;
			}
		}

		return AllowedObjects.Num() > 0;
	}

	template <CObjectInStreamInvocable TOnAllowed, CObjectInStreamInvocable TOnDisallowed>
	void FSyncControlState::EnumerateChanges(const FConcertReplication_ChangeSyncControl& Event, TOnAllowed&& OnAllowed, TOnDisallowed&& OnDisallowed)
	{
		for (const TPair<FConcertObjectInStreamID, bool>& NewControlState : Event.NewControlStates)
		{
			const FConcertObjectInStreamID& Object = NewControlState.Key;
			if (NewControlState.Value)
			{
				OnAllowed(Object);
			}
			else
			{
				OnDisallowed(Object);
			}
		}
	}

	template<CObjectInStreamInvocable TOnAllowed, CObjectInStreamInvocable TOnDisallowed>
	void FSyncControlState::AppendChanges(const FConcertReplication_ChangeSyncControl& Event, TOnAllowed&& OnAllowed, TOnDisallowed&& OnDisallowed)
	{
		EnumerateChanges(Event,
			[this, &OnAllowed](const FConcertObjectInStreamID& Object){ AllowedObjects.Add(Object); OnAllowed(Object); },
			[this, &OnDisallowed](const FConcertObjectInStreamID& Object){ AllowedObjects.Remove(Object); OnDisallowed(Object); }
			);
	}

	inline void FSyncControlState::AppendChanges(const FConcertReplication_ChangeSyncControl& Event)
	{
		AppendChanges(Event, [](const FConcertObjectInStreamID&){}, [](const FConcertObjectInStreamID&){});
	}
	
	template <CObjectInStreamInvocable TOnAllowed, CObjectInStreamInvocable TOnDisallowed>
	void FSyncControlState::AppendChanges(const FConcertReplication_ChangeAuthority_Request& Request, const FConcertReplication_ChangeAuthority_Response& Response, TOnAllowed&& OnAllowed, TOnDisallowed&& OnDisallowed)
	{
		for (const TPair<FSoftObjectPath, FConcertStreamArray>& ImplicitChange : Request.ReleaseAuthority)
		{
			for (const FGuid& StreamId : ImplicitChange.Value.StreamIds)
			{
				const FConcertObjectInStreamID ImplicitlyRemoved { StreamId, ImplicitChange.Key };

				const int32 NumRemoved = AllowedObjects.Remove(ImplicitlyRemoved);
				if (NumRemoved > 0)
				{
					OnDisallowed(ImplicitlyRemoved);
				}
			}
		}

		AppendChanges(Response.SyncControl, MoveTemp(OnAllowed), MoveTemp(OnDisallowed));
	}

	inline void FSyncControlState::AppendChanges(const FConcertReplication_ChangeAuthority_Request& Request, const FConcertReplication_ChangeAuthority_Response& Response)
	{
		AppendChanges(Request, Response, [](const FConcertObjectInStreamID&){}, [](const FConcertObjectInStreamID&){});
	}
	
	template <CObjectInStreamInvocable TOnDisallowed>
	void FSyncControlState::AppendChanges(const FConcertReplication_ChangeStream_Request& Request, TOnDisallowed&& OnDisallowed)
	{
		for (auto It = AllowedObjects.CreateIterator(); It; ++It)
		{
			if (Request.StreamsToRemove.Contains(It->StreamId))
			{
				const FConcertObjectInStreamID StreamId = *It;
				It.RemoveCurrent();
				OnDisallowed(StreamId);
			}
		}
		
		if (AllowedObjects.IsEmpty())
		{
			return;
		}

		for (const FConcertObjectInStreamID& RemovedObject : Request.ObjectsToRemove)
		{
			if (IsObjectAllowed(RemovedObject))
			{
				AllowedObjects.Remove(RemovedObject);
				OnDisallowed(RemovedObject);

				if (AllowedObjects.IsEmpty())
				{
					break;
				}
			}
		}
	}

	inline void FSyncControlState::AppendChanges(const FConcertReplication_ChangeStream_Request& Request)
	{
		AppendChanges(Request, [](const FConcertObjectInStreamID&){});
	}
}

