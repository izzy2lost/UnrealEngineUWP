// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncThread.h"

namespace unsync {

class FScheduler;
struct FTaskGroup;

extern FScheduler* GScheduler;

enum class EWorkloadType : uint8
{
	Foreground,
	Background,
	Download,
	Upload,
	FileIO,
};

#if UNSYNC_USE_CONCRT
struct FSchedulerSemaphore : FNativeSemaphore
{
	explicit FSchedulerSemaphore(FScheduler& InScheduler, uint32 MaxCount) : FNativeSemaphore(MaxCount) { UNSYNC_UNUSED(InScheduler); }
};
#else
struct FSchedulerSemaphore
{
	UNSYNC_DISALLOW_COPY_ASSIGN(FSchedulerSemaphore)

	FSchedulerSemaphore(FScheduler& InScheduler, uint32 MaxCount);

	void Acquire();
	void Release();

	FScheduler& Scheduler;

	std::counting_semaphore<UNSYNC_MAX_TOTAL_THREADS> Native;
};
#endif

class FScheduler
{
public:
	UNSYNC_DISALLOW_COPY_ASSIGN(FScheduler)

	static constexpr uint32 MAX_NETWORK_TASKS	 = 8;
	static constexpr uint32 MAX_FILESYSTEM_TASKS = 64;

	FScheduler(uint32 InNumWorkerThreads);
	~FScheduler();

	const uint32 NumWorkerThreads;

	FSchedulerSemaphore NetworkSempahore;
	FSchedulerSemaphore FilesystemSemaphore;

	FTaskGroup CreateTaskGroup(EWorkloadType Type = EWorkloadType::Foreground);

	void TryExecuteTask() { ThreadPool.TryExecuteTask(); }

private:
	FThreadPool ThreadPool;
};

#if UNSYNC_USE_CONCRT

struct FTaskGroup : concurrency::task_group
{
	UNSYNC_DISALLOW_COPY_ASSIGN(FTaskGroup)

private:
	friend FScheduler;
	FTaskGroup() : concurrency::task_group() {}
};

template<typename IT, typename FT>
inline void
ParallelForEach(IT ItBegin, IT ItEnd, FT F)
{
	concurrency::parallel_for_each(ItBegin, ItEnd, F);
}

#else // UNSYNC_USE_CONCRT

struct FTaskGroup
{
	UNSYNC_DISALLOW_COPY_ASSIGN(FTaskGroup)

	template<typename F>
	void run(F InFunction)
	{
		++NumStartedTasks;

		ThreadPool.PushTask(
			[&NumStartedTasks  = this->NumStartedTasks,
			 &NumFinishedTasks = this->NumFinishedTasks,
			 Function		   = std::forward<F>(InFunction)]() -> void
			{
				Function();
				++NumFinishedTasks;
			});
	}

	void wait()
	{
		while (NumFinishedTasks.load() != NumStartedTasks.load())
		{
			ThreadPool.TryExecuteTask();
		}
	}

	FThreadPool&		ThreadPool;
	std::atomic<uint64> NumStartedTasks;
	std::atomic<uint64> NumFinishedTasks;

	~FTaskGroup() { wait(); };

private:
	friend FScheduler;
	FTaskGroup(FThreadPool& InThreadPool) : ThreadPool(InThreadPool) {}
};

template<typename IT, typename FT>
inline void
ParallelForEach(IT ItBegin, IT ItEnd, FT F)
{
	FTaskGroup CreateTaskGroup = GScheduler->CreateTaskGroup();

	for (; ItBegin != ItEnd; ++ItBegin)
	{
		auto* It = &(*ItBegin);
		CreateTaskGroup.run([&F, It]() { F(*It); });
	}

	CreateTaskGroup.wait();
}

#endif // UNSYNC_USE_CONCRT

template<typename T, typename FT>
inline void
ParallelForEach(T& Container, FT F)
{
	ParallelForEach(std::begin(Container), std::end(Container), F);
}

}  // namespace unsync
