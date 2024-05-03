// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirtyNetObjectTracker.h"
#include "HAL/PlatformAtomics.h"
#include "Iris/IrisConstants.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Traits/IntType.h"
#include <atomic>

// Don't compile verbose logs in Shipping builds
#if (UE_BUILD_SHIPPING)
#	define UE_NET_DIRTYOBJECTTRACKER_LOG_COMPIL_VERBOSITY Log
#else
#	define UE_NET_DIRTYOBJECTTRACKER_LOG_COMPIL_VERBOSITY All
#endif 

DEFINE_LOG_CATEGORY_STATIC(LogIrisDirtyTracker, Log, UE_NET_DIRTYOBJECTTRACKER_LOG_COMPIL_VERBOSITY)

namespace UE::Net::Private
{

FDirtyNetObjectTracker::FDirtyNetObjectTracker()
: ReplicationSystemId(InvalidReplicationSystemId)
{
}

FDirtyNetObjectTracker::~FDirtyNetObjectTracker()
{
	Deinit();
}

void FDirtyNetObjectTracker::Init(const FDirtyNetObjectTrackerInitParams& Params)
{
	check(Params.NetRefHandleManager != nullptr);

	NetRefHandleManager = Params.NetRefHandleManager;
	ReplicationSystemId = Params.ReplicationSystemId;
	
	NetObjectIdCount = Params.MaxObjectCount;

	GlobalDirtyTrackerPollHandle = FGlobalDirtyNetObjectTracker::CreatePoller();

	AccumulatedDirtyNetObjects.Init(NetObjectIdCount);
	ForceNetUpdateObjects.Init(NetObjectIdCount);
	DirtyNetObjects.Init(NetObjectIdCount);

	AllowExternalAccess();

	UE_LOG(LogIrisDirtyTracker, Log, TEXT("FDirtyNetObjectTracker::Init[%u]: CurrentMaxSize: %u"), ReplicationSystemId, NetObjectIdCount);
}

void FDirtyNetObjectTracker::Deinit()
{
	GlobalDirtyTrackerPollHandle.Destroy();
	bShouldResetPolledGlobalDirtyTracker = false;
}

void FDirtyNetObjectTracker::GrabAndApplyGlobalDirtyObjectList()
{
	{
		const TSet<FNetHandle>& GlobalDirtyNetObjects = FGlobalDirtyNetObjectTracker::GetDirtyNetObjects(GlobalDirtyTrackerPollHandle);
		for (FNetHandle NetHandle : GlobalDirtyNetObjects)
		{
			const FInternalNetRefIndex NetObjectIndex = NetRefHandleManager->GetInternalIndexFromNetHandle(NetHandle);
			if (NetObjectIndex != FNetRefHandleManager::InvalidInternalIndex)
			{
				DirtyNetObjects.SetBit(NetObjectIndex);
			}
		}
	}

	FGlobalDirtyNetObjectTracker::ResetDirtyNetObjectsIfSinglePoller(GlobalDirtyTrackerPollHandle);

	bShouldResetPolledGlobalDirtyTracker = true;
}

void FDirtyNetObjectTracker::UpdateDirtyNetObjects()
{
	if (!GlobalDirtyTrackerPollHandle.IsValid())
	{
		return;
	}

	IRIS_PROFILER_SCOPE(FDirtyNetObjectTracker_UpdateDirtyNetObjects)

	LockExternalAccess();

	GrabAndApplyGlobalDirtyObjectList();

	//$IRIS TODO:  We could look if any objects where actually in the global list and skip the array iteration if not needed.

	const FNetBitArrayView GlobalScopeList = NetRefHandleManager->GetCurrentFrameScopableInternalIndices();
	const uint32* GlobalScopeListData = GlobalScopeList.GetData();
	
	uint32* AccumulatedDirtyNetObjectsData = AccumulatedDirtyNetObjects.GetData();
	uint32* DirtyNetObjectsData = DirtyNetObjects.GetData();

	const uint32 NumWords = AccumulatedDirtyNetObjects.GetNumWords();
	check(NumWords == DirtyNetObjects.GetNumWords());
	check(NumWords == GlobalScopeList.GetNumWords());

	for (uint32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
	{
		// Due to objects having been marked as dirty and later removed we must make sure that all dirty objects are still in scope.
		uint32 DirtyObjectWord = DirtyNetObjectsData[WordIndex] & GlobalScopeListData[WordIndex];
		DirtyNetObjectsData[WordIndex] = DirtyObjectWord;

		// Add the latest dirty objects to the accumulated list and remove no-longer scoped objects that have never been copied.
		AccumulatedDirtyNetObjectsData[WordIndex] = (AccumulatedDirtyNetObjectsData[WordIndex] | DirtyNetObjectsData[WordIndex]) & GlobalScopeListData[WordIndex];
	}

	AllowExternalAccess();
}

void FDirtyNetObjectTracker::UpdateAndLockDirtyNetObjects()
{
	if (!GlobalDirtyTrackerPollHandle.IsValid())
	{
		return;
	}
	
	UpdateDirtyNetObjects();

	FGlobalDirtyNetObjectTracker::LockDirtyListUntilReset(GlobalDirtyTrackerPollHandle);
}

void FDirtyNetObjectTracker::UpdateAccumulatedDirtyList()
{
	IRIS_PROFILER_SCOPE(FDirtyNetObjectTracker_UpdateDirtyNetObjects)
	AccumulatedDirtyNetObjects.Combine(DirtyNetObjects, FNetBitArray::OrOp);
}

void FDirtyNetObjectTracker::MarkNetObjectDirty(FInternalNetRefIndex NetObjectIndex)
{
#if UE_NET_THREAD_SAFETY_CHECK
	checkf(bIsExternalAccessAllowed, TEXT("Cannot mark objects dirty while the bitarray is locked for modifications."));
#endif

	if (NetObjectIndex >= NetObjectIdCount || NetObjectIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		UE_LOG(LogIrisDirtyTracker, Warning, TEXT("FDirtyNetObjectTracker::MarkNetObjectDirty received invalid NetObjectIndex: %u | Max: %u"), NetObjectIndex, NetObjectIdCount);
		return;
	}

	const uint32 BitOffset = NetObjectIndex;
	const StorageType BitMask = StorageType(1) << (BitOffset & (StorageTypeBitCount - 1));

	uint32* DirtyNetObjectsData = DirtyNetObjects.GetData();

	// ideally we'd have c++20 std::atomic_ref for this
	FPlatformAtomics::InterlockedOr((TSignedIntType<sizeof(StorageType)>::Type*)(&DirtyNetObjectsData[BitOffset/StorageTypeBitCount]), BitMask);

	UE_LOG(LogIrisDirtyTracker, Verbose, TEXT("FDirtyNetObjectTracker::MarkNetObjectDirty[%u]: %s"), ReplicationSystemId, *NetRefHandleManager->PrintObjectFromIndex(NetObjectIndex));
}

void FDirtyNetObjectTracker::ForceNetUpdate(FInternalNetRefIndex NetObjectIndex)
{
	ForceNetUpdateObjects.SetBit(NetObjectIndex);

	// Flag the object dirty so we update his filters too
	MarkNetObjectDirty(NetObjectIndex);

	UE_LOG(LogIrisDirtyTracker, Verbose, TEXT("FDirtyNetObjectTracker::ForceNetUpdateObjects[%u]: %s"), ReplicationSystemId, *NetRefHandleManager->PrintObjectFromIndex(NetObjectIndex));
}

void FDirtyNetObjectTracker::LockExternalAccess()
{
#if UE_NET_THREAD_SAFETY_CHECK
	bIsExternalAccessAllowed = false;
#endif
}

void FDirtyNetObjectTracker::AllowExternalAccess()
{
#if UE_NET_THREAD_SAFETY_CHECK
	bIsExternalAccessAllowed = true;
#endif
}

FNetBitArrayView FDirtyNetObjectTracker::GetDirtyNetObjectsThisFrame()
{
#if UE_NET_THREAD_SAFETY_CHECK
	checkf(!bIsExternalAccessAllowed, TEXT("Cannot access the DirtyNetObjects bitarray unless its locked for multithread access."));
#endif
	return MakeNetBitArrayView(DirtyNetObjects);
}

void FDirtyNetObjectTracker::ReconcilePolledList(const FNetBitArrayView& ObjectsPolled)
{
	LockExternalAccess();

	if (bShouldResetPolledGlobalDirtyTracker)
	{
		bShouldResetPolledGlobalDirtyTracker = false;
		FGlobalDirtyNetObjectTracker::ResetDirtyNetObjects(GlobalDirtyTrackerPollHandle);
	}

	// Clear ForceNetUpdate from every object that were polled.
	MakeNetBitArrayView(ForceNetUpdateObjects).Combine(ObjectsPolled, FNetBitArrayView::AndNotOp);

	// Clear dirty flags for objects that were polled
	MakeNetBitArrayView(AccumulatedDirtyNetObjects).Combine(ObjectsPolled, FNetBitArrayView::AndNotOp);

	// Clear the current frame dirty objects
	DirtyNetObjects.Reset();

	AllowExternalAccess();

	std::atomic_thread_fence(std::memory_order_seq_cst);
}

void MarkNetObjectStateDirty(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex)
{
	if (UReplicationSystem* ReplicationSystem = GetReplicationSystem(ReplicationSystemId))
	{
		FDirtyNetObjectTracker& DirtyNetObjectTracker = ReplicationSystem->GetReplicationSystemInternal()->GetDirtyNetObjectTracker();
		DirtyNetObjectTracker.MarkNetObjectDirty(NetObjectIndex);
	}
}

void ForceNetUpdate(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex)
{
	if (UReplicationSystem* ReplicationSystem = GetReplicationSystem(ReplicationSystemId))
	{
		FDirtyNetObjectTracker& DirtyNetObjectTracker = ReplicationSystem->GetReplicationSystemInternal()->GetDirtyNetObjectTracker();
		DirtyNetObjectTracker.ForceNetUpdate(NetObjectIndex);
	}
}

}
