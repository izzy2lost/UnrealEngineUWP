// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaScheduler.h"
#include "UbaProcess.h"
#include "UbaProcessStartInfoHolder.h"
#include "UbaSessionServer.h"
#include "UbaStringBuffer.h"

namespace uba
{
	struct Scheduler::QueuedProcess : ProcessStartInfoHolder
	{
		QueuedProcess(const ProcessStartInfo& si, const u8* ki, u32 kic)
		:	ProcessStartInfoHolder(si)
		, knownInputs(ki)
		, knownInputsCount(kic)
		{
		};

		const u8* knownInputs;
		u32 knownInputsCount;
	};

	struct Scheduler::ExitProcessInfo
	{
		Scheduler* scheduler = nullptr;
		ProcessStartInfo::ExitedCallback* originalExitedFunc = nullptr;
		void* originalExitedData = nullptr;
		const u8* knownInputs = nullptr;
		u32 knownInputsCount = 0;
		bool wasReturned = false;
		bool isRemote = false;
	};

	Scheduler::Scheduler(SessionServer& session, u32 maxLocalProcessors)
	:	m_session(session)
	,	m_maxLocalProcessors(maxLocalProcessors ? maxLocalProcessors : GetLogicalProcessorCount())
	,	m_updateThreadLoop(false)
	{
	}

	Scheduler::~Scheduler()
	{
		Stop();
	}

	void Scheduler::Start()
	{
		m_loop = true;
		m_thread.Start([this]() { ThreadLoop(); return 0; });

		m_session.SetRemoteProcessReturnedEvent([this](Process& process) { RemoteProcessReturned(process); });
		m_session.SetRemoteProcessSlotAvailableEvent([this]() { RemoteSlotAvailable(); });
	}

	void Scheduler::Stop()
	{
		m_loop = false;
		m_updateThreadLoop.Set();
		m_thread.Wait();
		m_session.WaitOnAllTasks();
	}

	u32 Scheduler::EnqueueProcess(const ProcessStartInfo& info, float weight, const void* knownInputs, u32 knownInputsBytes, u32 knownInputsCount)
	{
		UBA_ASSERTF(weight == 1.0f, TC("weight not implemented"));

		if (knownInputsCount)
		{
			void* ki = new u8[knownInputsBytes];
			memcpy(ki, knownInputs, knownInputsBytes);
			knownInputs = ki;
		}
		else
			knownInputs = nullptr;

		ScopedWriteLock lock(m_queuedProcessesLock);
		m_queuedProcesses.emplace_back(info, (const u8*)knownInputs, knownInputsCount);
		m_updateThreadLoop.Set();
		return 0;
	}

	void Scheduler::GetStats(u32& outQueued, u32& outActive, u32& outFinished)
	{
		outActive = m_activeLocalProcesses + m_activeRemoteProcesses;
		outFinished = m_finishedProcesses;
		ScopedReadLock lock(m_queuedProcessesLock);
		outQueued = u32(m_queuedProcesses.size());
	}

	void Scheduler::ThreadLoop()
	{
		while (m_loop)
		{
			if (!m_updateThreadLoop.IsSet())
				break;

			while (m_activeLocalProcesses < m_maxLocalProcessors)
				if (!RunQueuedProcess(true))
					break;
		}
	}

	void Scheduler::RemoteProcessReturned(Process& process)
	{
		auto& ei = *(ExitProcessInfo*)process.GetStartInfo().exitedUserData;

		ScopedWriteLock lock(m_queuedProcessesLock);
		ProcessStartInfo info = process.GetStartInfo();
		info.exitedFunc = ei.originalExitedFunc;
		info.exitedUserData = ei.originalExitedData;
		m_queuedProcesses.emplace_front(info, ei.knownInputs, ei.knownInputsCount);
		ei.knownInputs = nullptr;
		lock.Leave();

		ei.wasReturned = true;
		process.Cancel(true);
		--m_activeRemoteProcesses;
		m_updateThreadLoop.Set();
	}

	void Scheduler::RemoteSlotAvailable()
	{
		RunQueuedProcess(false);
	}

	void Scheduler::ProcessExited(ExitProcessInfo* info, const ProcessHandle& handle)
	{
		auto ig = MakeGuard([info]() { delete[] info->knownInputs;  delete info; });

		if (info->wasReturned)
			return;

		if (info->originalExitedFunc)
			info->originalExitedFunc(info->originalExitedData, handle);

		++m_finishedProcesses;

		if (info->isRemote)
		{
			--m_activeRemoteProcesses;
		}
		else
		{
			--m_activeLocalProcesses;
		}
		m_updateThreadLoop.Set();
	}

	bool Scheduler::RunQueuedProcess(bool runLocal)
	{
		ScopedWriteLock lock(m_queuedProcessesLock);
		if (m_queuedProcesses.empty())
			return false;
		auto si = m_queuedProcesses.front();
		m_queuedProcesses.pop_front();
		lock.Leave();

		auto exitInfo = new ExitProcessInfo();
		exitInfo->scheduler = this;
		exitInfo->originalExitedFunc = si.startInfo.exitedFunc;
		exitInfo->originalExitedData = si.startInfo.exitedUserData;
		exitInfo->knownInputs = si.knownInputs;
		exitInfo->knownInputsCount = si.knownInputsCount;
		exitInfo->isRemote = !runLocal;
		si.startInfo.exitedUserData = exitInfo;
		si.startInfo.exitedFunc = [](void* userData, const ProcessHandle& handle) { auto ei = (ExitProcessInfo*)userData; ei->scheduler->ProcessExited(ei, handle); };

		if (runLocal)
		{
			++m_activeLocalProcesses;
			m_session.RunProcess(si.startInfo);
		}
		else
		{
			++m_activeRemoteProcesses;
			m_session.RunProcessRemote(si.startInfo, 1.0f, si.knownInputs, si.knownInputsCount);
		}
		return true;
	}
}
