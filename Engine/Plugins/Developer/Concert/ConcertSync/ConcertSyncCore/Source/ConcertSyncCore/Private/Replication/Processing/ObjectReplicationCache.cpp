// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationCache.h"

#include "Replication/ReplicationStreamObjectID.h"
#include "Replication/Formats/IObjectReplicationFormat.h"
#include "Replication/Messages/ConcertReplicationEvents.h"

namespace UE::ConcertSyncCore
{
	FObjectReplicationCache::FObjectReplicationCache(TSharedRef<IObjectReplicationFormat> ReplicationFormat)
		: ReplicationFormat(MoveTemp(ReplicationFormat))
	{}

	int32 FObjectReplicationCache::StoreUntilConsumed(const FGuid& OriginStreamId, const FConcertObjectReplicationEvent& ObjectReplicationEvent)
	{
		int32 NumAccepted = 0;
		
		const FReplicationStreamObjectID ObjectId{ OriginStreamId, ObjectReplicationEvent.ReplicatedObject };
		FObjectCache* ObjectCacheBeforeAddition = Cache.Find(ObjectId);
		if (ObjectCacheBeforeAddition)
		{
			// It is important to compare by address and not by TWeakPtr instance because each user has a different TWeakPtr instance (but they all point to the same memory).
			TSet<FConcertObjectReplicationEvent*> CombineOnceDetection;
			for (const TPair<TWeakPtr<IReplicationCacheUser>, TWeakPtr<FConcertObjectReplicationEvent>>& InUseDataPair : ObjectCacheBeforeAddition->DataInUse)
			{
				const TWeakPtr<FConcertObjectReplicationEvent>& EventData = InUseDataPair.Value;
				const TSharedPtr<FConcertObjectReplicationEvent> EventDataPin = EventData.Pin();
				if (EventDataPin && !CombineOnceDetection.Contains(EventDataPin.Get()))
				{
					CombineOnceDetection.Add(EventDataPin.Get());
					ReplicationFormat->CombineReplicationEvents(EventDataPin->SerializedPayload, ObjectReplicationEvent.SerializedPayload);
				}
			}
		}

		FObjectCache* ObjectCacheAfterAddition = ObjectCacheBeforeAddition ? ObjectCacheBeforeAddition : nullptr;
		TSharedPtr<FConcertObjectReplicationEvent> LazilyCopiedEventPtr;
		for (const TSharedRef<IReplicationCacheUser>& CacheUser : CacheUsers)
		{
			if (ObjectCacheBeforeAddition && ObjectCacheBeforeAddition->DataInUse.Contains(CacheUser))
			{
				continue;
			}

			if (CacheUser->WantsToAcceptObject(ObjectId))
			{
				++NumAccepted;
				
				// Do the event copy only when somebody wants the data...
				if (!LazilyCopiedEventPtr.IsValid())
				{
					LazilyCopiedEventPtr = MakeShared<FConcertObjectReplicationEvent>(ObjectReplicationEvent);
				}

				// We'll constructor a new proxy shared ptr which will clean-up Cache when released
				TWeakPtr<IReplicationCacheUser> WeakUserPtr = CacheUser;
				TWeakPtr<FObjectReplicationCache> WeakThisPtr = AsWeak();
				TSharedRef<FConcertObjectReplicationEvent> GuardPtr = MakeShareable(LazilyCopiedEventPtr.Get(), [OriginStreamId, LazilyCopiedEventPtr, WeakUserPtr, WeakThisPtr](auto*)
				{
					// Replication cache user can survive the destruction of the cache because it is created externally
					const TSharedPtr<FObjectReplicationCache> This = WeakThisPtr.Pin();
					if (!This)
					{
						return;
					}

					// ObjectCache may not be found because cache user was unregistered and then destroyed: UnregisterDataCacheUser removes cache users.
					const FReplicationStreamObjectID ObjectId{ OriginStreamId, LazilyCopiedEventPtr->ReplicatedObject };
					FObjectCache* ObjectCache = This->Cache.Find(ObjectId);
					if (!ObjectCache)
					{
						return;
					}
					
					ObjectCache->DataInUse.Remove(WeakUserPtr);
					if (ObjectCache->DataInUse.IsEmpty())
					{
						This->Cache.Remove(ObjectId);
					}
					
					// LazyCopiedEventPtr will decrement counter and possibly be destroyed now
				});
				
				const TWeakPtr<FConcertObjectReplicationEvent> WeakGuardPtr = GuardPtr;
				CacheUser->OnDataCached(ObjectId, MoveTemp(GuardPtr));
				// Was it instantly consumed? Should not really happen but it could technically...
				if (!LIKELY(WeakGuardPtr.IsValid()))
				{
					continue;
				}
				
				if (!ObjectCacheAfterAddition)
				{
					ObjectCacheAfterAddition = &Cache.Add(ObjectId);
				}
				ObjectCacheAfterAddition->DataInUse.Add(WeakUserPtr, WeakGuardPtr);
			}
		}

		return NumAccepted;
	}

	void FObjectReplicationCache::RegisterDataCacheUser(TSharedRef<IReplicationCacheUser> User)
	{
		if (!CacheUsers.Contains(User))
		{
			CacheUsers.Emplace(MoveTemp(User));
		}
	}

	void FObjectReplicationCache::UnregisterDataCacheUser(const TSharedRef<IReplicationCacheUser>& User)
	{
		CacheUsers.RemoveSingle(User);
		
		for (auto It = Cache.CreateIterator(); It; ++It)
		{
			FObjectCache& ObjectCache = It->Value;
			ObjectCache.DataInUse.Remove(User);
			if (ObjectCache.DataInUse.IsEmpty())
			{
				It.RemoveCurrent();
			}
		}
	}
}
