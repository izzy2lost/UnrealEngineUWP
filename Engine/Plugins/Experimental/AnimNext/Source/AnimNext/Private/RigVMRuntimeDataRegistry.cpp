// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMRuntimeDataRegistry.h"
#include "Misc/ScopeRWLock.h"

namespace UE::AnimNext
{

namespace Private
{

static bool bInitialized = false;
static FDelegateHandle PostGarbageCollectHandle;

static std::atomic<uint32> GCCycle = 0; // Main thread GC counter, incremented on each main thread GC cycle

static thread_local uint32 LocalGCCycle = 0;	// Local thread GC counter, used to compare with main and trigger compaction if different
static thread_local TMap<FRigVMRuntimeDataID, FRigVMRuntimeData> RuntimeDataStorage;

} // end namespace Private


/*static*/ void FRigVMRuntimeDataRegistry::Init()
{
	if (!Private::bInitialized)
	{
		Private::PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic(&FRigVMRuntimeDataRegistry::HandlePostGarbageCollect);

		Private::bInitialized = true;
	}
}

/*static*/ void FRigVMRuntimeDataRegistry::Destroy()
{
	if (Private::bInitialized)
	{
		Private::bInitialized = false;

		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(Private::PostGarbageCollectHandle);
		Private::RuntimeDataStorage.Empty();
	}
}

/*static*/ FRigVMRuntimeData* FRigVMRuntimeDataRegistry::FindRuntimeData(const FRigVMRuntimeDataID& RigVMRuntimeDataID)
{
	check(Private::bInitialized);

	const uint32 CurrentCycle = Private::GCCycle;
	if (CurrentCycle != Private::LocalGCCycle)
	{
		PerformStorageCompaction();

		Private::LocalGCCycle = CurrentCycle;
	}

	return Private::RuntimeDataStorage.Find(RigVMRuntimeDataID);
}

/*static*/ FRigVMRuntimeData* FRigVMRuntimeDataRegistry::AddRuntimeData(const FRigVMRuntimeDataID& RigVMRuntimeDataID, const FRigVMExtendedExecuteContext& ReferenceContext)
{
	check(Private::bInitialized);

	FRigVMRuntimeData* RigVMRuntimeData = nullptr;

	FRigVMRuntimeData& NewRigVMRuntimeData = Private::RuntimeDataStorage.Add(RigVMRuntimeDataID, FRigVMRuntimeData());
	NewRigVMRuntimeData.Context = ReferenceContext;
	RigVMRuntimeData = &NewRigVMRuntimeData;

	return RigVMRuntimeData;
}

/*static*/ FRigVMRuntimeData* FRigVMRuntimeDataRegistry::FindOrAddRuntimeData(const FRigVMRuntimeDataID& RigVMRuntimeDataID, const FRigVMExtendedExecuteContext& ReferenceContext)
{
	check(Private::bInitialized);

	FRigVMRuntimeData* RigVMRuntimeData = nullptr;

	if (RigVMRuntimeData = FindRuntimeData(RigVMRuntimeDataID); RigVMRuntimeData != nullptr)
	{
		if (RigVMRuntimeData->Context.VMHash != ReferenceContext.VMHash)
		{
			RigVMRuntimeData->Context = ReferenceContext;
		}

		return RigVMRuntimeData;
	}

	return AddRuntimeData(RigVMRuntimeDataID, ReferenceContext);
}

/*static*/ void FRigVMRuntimeDataRegistry::HandlePostGarbageCollect()
{
	Private::GCCycle++;
	Private::LocalGCCycle = Private::GCCycle; // avoid additional compactions on main thread

	PerformStorageCompaction();
}

/*static*/ void FRigVMRuntimeDataRegistry::PerformStorageCompaction()
{
	for (auto Iter = Private::RuntimeDataStorage.CreateIterator(); Iter; ++Iter)
	{
		const FRigVMRuntimeDataID& RuntimeDataID = Iter.Key();
		if (RuntimeDataID.ResolveObjectPtr() == nullptr)
		{
			Iter.RemoveCurrent();
		}
	}
}

} // end namespace UE::AnimNext
