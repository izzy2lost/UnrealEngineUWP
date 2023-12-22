// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaMemory.h"

namespace uba
{
	class WorkManager
	{
	public:
		virtual void AddWork(const Function<void()>& work, u32 count, const tchar* desc) = 0;
		virtual u32 GetWorkerCount() = 0;
	};


	class WorkManagerImpl : public WorkManager
	{
	public:
		WorkManagerImpl(u32 workerCount);
		virtual ~WorkManagerImpl();
		virtual void AddWork(const Function<void()>& work, u32 count, const tchar* desc) override;
		virtual u32 GetWorkerCount() override;
		void DoWork(u32 count = 1);
		void FlushWork();

	private:
		struct Worker;
		void PushWorker(Worker* worker);
		void PushWorkerNoLock(Worker* worker);
		Worker* PopWorkerNoLock();

		Vector<Worker*> m_workers;
		struct Work { Function<void()> func; };
		ReaderWriterLock m_workLock;
		List<Work> m_work;
		Atomic<u32> m_activeWorkerCount;

		ReaderWriterLock m_availableWorkersLock;
		Worker* m_firstAvailableWorker = nullptr;
	};
}
