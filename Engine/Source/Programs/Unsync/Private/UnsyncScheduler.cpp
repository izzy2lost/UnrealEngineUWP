// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncScheduler.h"

namespace unsync {

FScheduler* GScheduler = nullptr;

FScheduler::FScheduler(uint32 InNumWorkerThreads)
: NumWorkerThreads(InNumWorkerThreads)
, NetworkSempahore(*this, MAX_NETWORK_TASKS)
, FilesystemSemaphore(*this, MAX_FILESYSTEM_TASKS)
{
#if UNSYNC_USE_CONCRT
	UNSYNC_VERBOSE(L"Scheduler: PPL");
#else
	UNSYNC_VERBOSE(L"Scheduler: Custom");
	ThreadPool.StartWorkers(NumWorkerThreads);
#endif
}

FScheduler::~FScheduler()
{
}

FTaskGroup FScheduler::CreateTaskGroup(EWorkloadType Type)
{
	// TODO: use separate thread pools for tasks of different types
	UNSYNC_UNUSED(Type);

#if UNSYNC_USE_CONCRT
	return FTaskGroup();
#else
	return FTaskGroup(ThreadPool);
#endif
}

#if !UNSYNC_USE_CONCRT

FSchedulerSemaphore::FSchedulerSemaphore(FScheduler& InScheduler, uint32 MaxCount)
: Scheduler(InScheduler)
, Native(std::min(MaxCount, 1 + InScheduler.NumWorkerThreads)) // 1 for main thread + N workers
{
}

void
FSchedulerSemaphore::Acquire()
{
	while (!Native.try_acquire())
	{
		Scheduler.TryExecuteTask();
	}
}

void
FSchedulerSemaphore::Release()
{
	Native.release();
}

#endif

}  // namespace unsync
