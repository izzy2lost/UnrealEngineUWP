// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaMemory.h"
#include "UbaProcessStartInfo.h"
#include "UbaThread.h"

namespace uba
{
	class Process;
	class SessionServer;
	struct ProcessStartInfoHolder;

	// This is a simple scheduler that does not handle any dependencies. Can be added based on need

	class Scheduler
	{
	public:
		Scheduler(SessionServer& session, u32 maxLocalProcessors = ~0u, bool enableProcessReuse = false);
		~Scheduler();

		void Start();
		void Stop();

		u32 EnqueueProcess(const ProcessStartInfo& info, float weight = 1.0f, const void* knownInputs = nullptr, u32 knownInputsBytes = 0, u32 knownInputsCount = 0);

		void GetStats(u32& outQueued, u32& outActive, u32& outFinished);
	private:
		struct ExitProcessInfo;

		void ThreadLoop();
		void RemoteProcessReturned(Process& process);
		void RemoteSlotAvailable();
		void ProcessExited(ExitProcessInfo* info, const ProcessHandle& handle);
		bool RunQueuedProcess(bool runLocal);
		u32 HandleReuseMessage(Process& process, const void* recv, u32 recvSize, void* send, u32 sendCapacity);

		SessionServer& m_session;
		u32 m_maxLocalProcessors;

		ReaderWriterLock m_queuedProcessesLock;
		struct QueuedProcess;
		List<QueuedProcess> m_queuedProcesses;
		Event m_updateThreadLoop;
		Thread m_thread;
		bool m_loop = false;
		bool m_enableProcessReuse;

		Atomic<u32> m_activeLocalProcesses;
		Atomic<u32> m_activeRemoteProcesses;
		Atomic<u32> m_finishedProcesses;

		Scheduler(const Scheduler&) = delete;
		void operator=(const Scheduler&) = delete;
	};
}
