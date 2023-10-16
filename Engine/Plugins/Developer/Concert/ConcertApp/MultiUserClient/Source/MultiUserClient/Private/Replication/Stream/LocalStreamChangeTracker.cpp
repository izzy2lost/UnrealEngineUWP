// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalStreamChangeTracker.h"

#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FLocalClientStreamDiffer"

namespace UE::MultiUserClient
{
	FLocalStreamChangeTracker::FLocalStreamChangeTracker(
		IClientStreamSynchronizer& InStreamSynchronizer,
		TAttribute<FObjectReplicationMap*> InStreamWithInProgressChangesAttribute,
		FOnModifyReplicationMap InOnModifyReplicationMapDelegate
		)
		: StreamSynchronizer(InStreamSynchronizer)
		, StreamWithInProgressChangesAttribute(MoveTemp(InStreamWithInProgressChangesAttribute))
		, OnModifyReplicationMapDelegate(MoveTemp(InOnModifyReplicationMapDelegate))
	{
		check(InStreamWithInProgressChangesAttribute.IsBound() || InStreamWithInProgressChangesAttribute.IsSet());
		StreamSynchronizer.OnServerStateChanged().AddRaw(this, &FLocalStreamChangeTracker::RefreshChangesCache);
	}

	FLocalStreamChangeTracker::~FLocalStreamChangeTracker()
	{
		StreamSynchronizer.OnServerStateChanged().RemoveAll(this);
	}

	void FLocalStreamChangeTracker::RefreshChangesCache()
	{
		CachedDeltaChange = DiffChanges();
		RefreshWarnings();
	}

	TFuture<FSubmitChangesResult> FLocalStreamChangeTracker::SubmitChanges(bool bRefreshChanges)
	{
		if (bRefreshChanges)
		{
			RefreshChangesCache();
		}
		return StreamSynchronizer.SubmitChanges(CachedDeltaChange);
	}

	void FLocalStreamChangeTracker::RevertCachedChanges()
	{
		if (HasRevertableLocalChanges())
		{
			FScopedTransaction Transaction(LOCTEXT("RevertCachedChanges", "Revert replication changes"));
			OnModifyReplicationMapDelegate.ExecuteIfBound();
			
			*StreamWithInProgressChangesAttribute.Get() = StreamSynchronizer.GetServerState();
			CachedDeltaChange = {};

			check(IsInGameThread());
			OnChangesRevertedDelegate.Broadcast();
			RefreshChangesCache();
		}
	}

	bool FLocalStreamChangeTracker::HasSubmittableLocalChanges() const
	{
		return !CachedDeltaChange.ObjectsToPut.IsEmpty() || !CachedDeltaChange.ObjectsToRemove.IsEmpty();
	}

	bool FLocalStreamChangeTracker::HasRevertableLocalChanges() const
	{
		return HasSubmittableLocalChanges() || HasAnyWarnings();
	}

	bool FLocalStreamChangeTracker::CanMakeSubmitNetworkRequest() const
	{
		return StreamSynchronizer.CanMakeSubmitRequest();
	}

	EObjectWarningFlags FLocalStreamChangeTracker::GetObjectWarningFlags(const FSoftObjectPath& ObjectPath)
	{
		const EObjectWarningFlags* Flags = ObjectsWithWarnings.Find(ObjectPath);
		return Flags ? *Flags : EObjectWarningFlags::Ok;
	}

	bool FLocalStreamChangeTracker::HasAnyWarnings() const
	{
		return !ObjectsWithWarnings.IsEmpty();
	}

	FLocalStreamChangeTracker::EObjectChangeType FLocalStreamChangeTracker::GetObjectChanges(const FSoftObjectPath& Object) const
	{
		// TODO DP:
		return EObjectChangeType::NoChange;
	}

	FLocalStreamChangeTracker::EPropertyChangeType FLocalStreamChangeTracker::GetPropertyChanges(const FSoftObjectPath& Object, const FConcertPropertyChain& PropertyChain) const
	{
		// TODO DP:
		return EPropertyChangeType::NoChange;
	}
	
	FStreamChangelist FLocalStreamChangeTracker::DiffChanges(
		const FGuid& StreamId,
		const FObjectReplicationMap& Base,
		const FObjectReplicationMap& Changed
		)
	{
		// BuildRequestFromDiff does not tolerate any invalid entries (like empty properties, which the UI generates right after you add an object to the list)
		FObjectReplicationMap Cleansed = Changed;
		ConcertSyncCore::Replication::ChangeStreamUtils::IterateInvalidEntries(Changed, [&Cleansed](const FSoftObjectPath& InvalidObject, const FReplicatedObjectInfo&)
		{
			Cleansed.ReplicatedObjects.Remove(InvalidObject);
			return EBreakBehavior::Continue;
		});

		FConcertReplication_ChangeStream_Request Request = ConcertSyncCore::Replication::ChangeStreamUtils::BuildRequestFromDiff(StreamId, Base, Cleansed);
		return { MoveTemp(Request.ObjectsToRemove), MoveTemp(Request.ObjectsToPut) };
	}

	void FLocalStreamChangeTracker::RefreshWarnings()
	{
		ObjectsWithWarnings.Empty();
		const FObjectReplicationMap* Map = StreamWithInProgressChangesAttribute.Get();
		if (!Map)
		{
			return;
		}
		
		ConcertSyncCore::Replication::ChangeStreamUtils::IterateInvalidEntries(*Map, [this](const FSoftObjectPath& InvalidObject, const FReplicatedObjectInfo& Info)
		{
			EObjectWarningFlags Flags = EObjectWarningFlags::Ok;
			if (Info.PropertySelection.ReplicatedProperties.IsEmpty())
			{
				Flags |= EObjectWarningFlags::MissingProperties;
			}
			
			ObjectsWithWarnings.Add(InvalidObject, Flags);
			return EBreakBehavior::Continue;
		});
	}
}

#undef LOCTEXT_NAMESPACE