// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncThread.h"

#include <chrono>

namespace unsync {

uint32 GMaxThreads = std::min<uint32>(UNSYNC_MAX_TOTAL_THREADS, std::thread::hardware_concurrency());

FThreadPool GThreadPool;

void
FThreadPool::StartWorkers(uint32 NumWorkers)
{
	std::unique_lock<std::mutex> LockScope(Mutex);

	while (Threads.size() < NumWorkers)
	{
		Threads.emplace_back(
			[this]()
			{
				while (DoWork(true))
				{
				}
			});
	}
}

FThreadPool::~FThreadPool()
{
	bShutdown = true;
	WorkerWakeupCondition.notify_all();

	for (std::thread& Thread : Threads)
	{
		Thread.join();
	}
}

FThreadPool::FTaskFunction
FThreadPool::PopTask(bool bWaitForSignal)
{
	std::unique_lock<std::mutex> LockScope(Mutex);

	if (bWaitForSignal)
	{
		auto WaitUntil = [this]() { return bShutdown || !Tasks.empty(); };
		WorkerWakeupCondition.wait(LockScope, WaitUntil);
	}

	FThreadPool::FTaskFunction Result;

	if (!Tasks.empty())
	{
		Result = std::move(Tasks.front());
		Tasks.pop_front();
	}

	return Result;
}

uint64
FThreadPool::PushTask(FTaskFunction&& Fun)
{
	if (Threads.empty())
	{
		Fun();
		return NumTasksPushed;
	}
	else
	{
		std::unique_lock<std::mutex> LockScope(Mutex);
		Tasks.push_back(std::forward<FTaskFunction>(Fun));
		WorkerWakeupCondition.notify_one();
		return ++NumTasksPushed;
	}
}

bool
FThreadPool::DoWork(bool bWaitForSignal)
{
	FTaskFunction Task = PopTask(bWaitForSignal);

	if (Task)
	{
		Task();
		++NumTasksCompleted;
		return true;
	}
	else
	{
		return false;
	}
}

void
FThreadPool::WaitForFence(uint64 FenceValue)
{
	while (NumTasksCompleted < FenceValue)
	{
		if (DoWork(false))
		{
			continue;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

#if UNSYNC_USE_CONCRT
FConcurrencyPolicyScope::FConcurrencyPolicyScope(uint32 MaxConcurrency)
{
	auto Policy = Concurrency::CurrentScheduler::GetPolicy();

	const uint32 CurrentMaxConcurrency = Policy.GetPolicyValue(Concurrency::PolicyElementKey::MaxConcurrency);
	const uint32 CurrentMinConcurrency = Policy.GetPolicyValue(Concurrency::PolicyElementKey::MinConcurrency);

	MaxConcurrency = std::min(MaxConcurrency, std::thread::hardware_concurrency());
	MaxConcurrency = std::min(MaxConcurrency, CurrentMaxConcurrency);
	MaxConcurrency = std::max(1u, MaxConcurrency);

	Policy.SetConcurrencyLimits(CurrentMinConcurrency, MaxConcurrency);

	Concurrency::CurrentScheduler::Create(Policy);
}

FConcurrencyPolicyScope::~FConcurrencyPolicyScope()
{
	Concurrency::CurrentScheduler::Detach();
}

void
SchedulerSleep(uint32 Milliseconds)
{
	concurrency::event E;
	E.reset();
	E.wait(Milliseconds);
}

void
SchedulerYield()
{
	concurrency::Context::YieldExecution();
}

#else  // UNSYNC_USE_CONCRT

FConcurrencyPolicyScope::FConcurrencyPolicyScope(uint32 MaxConcurrency)
{
	// TODO
}

FConcurrencyPolicyScope::~FConcurrencyPolicyScope()
{
	// TODO
}

void
SchedulerSleep(uint32 Milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));
}

void
SchedulerYield()
{
	// TODO
}

#endif	// UNSYNC_USE_CONCRT

}  // namespace unsync
