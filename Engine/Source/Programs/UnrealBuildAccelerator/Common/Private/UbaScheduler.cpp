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
		void* originalUserData = nullptr;
		const u8* knownInputs = nullptr;
		u32 knownInputsCount = 0;
		bool wasReturned = false;
		bool isRemote = false;

		// If reusing processes then these are the actual info of the process
		TString lastArguments;
		TString lastWorkingDir;
		TString lastDescription;
	};

	Scheduler::Scheduler(SessionServer& session, u32 maxLocalProcessors, bool enableProcessReuse)
	:	m_session(session)
	,	m_maxLocalProcessors(maxLocalProcessors ? maxLocalProcessors : GetLogicalProcessorCount())
	,	m_updateThreadLoop(false)
	,	m_enableProcessReuse(enableProcessReuse)
	{
		session.RegisterCustomService([this](Process& process, const void* recv, u32 recvSize, void* send, u32 sendCapacity)
			{
				return HandleReuseMessage(process, recv, recvSize, send, sendCapacity);
			});
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
		auto& ei = *(ExitProcessInfo*)process.GetStartInfo().userData;

		ScopedWriteLock lock(m_queuedProcessesLock);
		ProcessStartInfo info = process.GetStartInfo();
		info.exitedFunc = ei.originalExitedFunc;
		info.userData = ei.originalUserData;
		if (!ei.lastArguments.empty())
		{
			info.arguments = ei.lastArguments.c_str();
			info.workingDir = ei.lastWorkingDir.c_str();
			info.description = ei.lastDescription.c_str();
		}
		m_queuedProcesses.emplace_front(info, ei.knownInputs, ei.knownInputsCount);
		ei.knownInputsCount = 0;
		ei.knownInputs = nullptr;
		ei.wasReturned = true;
		lock.Leave();

		process.Cancel(true); // Cancel will call ProcessExited
		--m_activeRemoteProcesses;
		m_updateThreadLoop.Set();
	}

	void Scheduler::RemoteSlotAvailable()
	{
		RunQueuedProcess(false);
	}

	void Scheduler::ProcessExited(ExitProcessInfo* info, const ProcessHandle& handle)
	{
		auto ig = MakeGuard([info]() { delete[] info->knownInputs; delete info; });

		if (info->wasReturned)
			return;

		if (auto func = info->originalExitedFunc)
			func(info->originalUserData, handle);

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
		if (runLocal)
			++m_activeLocalProcesses;
		lock.Leave();

		auto exitInfo = new ExitProcessInfo();
		exitInfo->scheduler = this;
		exitInfo->originalExitedFunc = si.startInfo.exitedFunc;
		exitInfo->originalUserData = si.startInfo.userData;
		exitInfo->knownInputs = si.knownInputs;
		exitInfo->knownInputsCount = si.knownInputsCount;
		exitInfo->isRemote = !runLocal;
		si.startInfo.userData = exitInfo;
		si.startInfo.exitedFunc = [](void* userData, const ProcessHandle& handle)
			{
				auto ei = (ExitProcessInfo*)userData;
				ei->scheduler->ProcessExited(ei, handle);
			};

		if (runLocal)
		{
			m_session.RunProcess(si.startInfo);
		}
		else
		{
			++m_activeRemoteProcesses;
			m_session.RunProcessRemote(si.startInfo, 1.0f, si.knownInputs, si.knownInputsCount);
		}
		return true;
	}

	u32 Scheduler::HandleReuseMessage(Process& process, const void* recv, u32 recvSize, void* send, u32 sendCapacity)
	{
		if (!m_enableProcessReuse)
			return 0;

		auto& currentStartInfo = process.GetStartInfo();
		auto info = (ExitProcessInfo*)currentStartInfo.userData;
		if (!info) // If null, process has already exited from some other thread
			return 0;

		// Call ExitedFunc and cleanup
		if (auto func = info->originalExitedFunc)
		{
			UBA_ASSERT(!info->wasReturned);
			ProcessHandle h;
			h.m_process = &process;
			func(info->originalUserData, h);
			h.m_process = nullptr;
		}
		info->knownInputsCount = 0;
		delete[] info->knownInputs;
		info->knownInputs = nullptr;
		info->originalExitedFunc = nullptr;
		info->originalUserData = nullptr;

		info->lastArguments.clear();
		info->lastWorkingDir.clear();
		info->lastDescription.clear();

		// Try to get queued process to send back
		ScopedWriteLock lock(m_queuedProcessesLock);
		if (info->wasReturned)
			return 0;
		if (m_queuedProcesses.empty())
			return 0;
		auto qp = m_queuedProcesses.front();
		m_queuedProcesses.pop_front();
		lock.Leave();

		delete[] qp.knownInputs;
		auto& si = qp.startInfo;

		UBA_ASSERT(Equals(currentStartInfo.application, si.application));

		// Move over exited func and user data for the queued process
		info->originalExitedFunc = si.exitedFunc;
		info->originalUserData = si.userData;
		info->lastArguments = si.arguments;
		info->lastWorkingDir = si.workingDir;
		info->lastDescription = si.description;

		// TODO: Don't think we need to udpate the other parts of StartInfo

		BinaryWriter writer((u8*)send, 0, sendCapacity);
		writer.WriteString(si.arguments);
		writer.WriteString(si.workingDir);
		writer.WriteString(si.description);
		return u32(writer.GetPosition());
	}
}
