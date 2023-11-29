// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaWorkManager.h"
#include "UbaEvent.h"
#include "UbaPlatform.h"
#include "UbaThread.h"

namespace uba
{
	struct WorkManagerImpl::Worker
	{
		Worker(WorkManagerImpl& manager) : m_workAvailable(false)
		{
			m_loop = true;
			m_thread.Start([&]() { ThreadWorker(manager); return 0; });
		}
		~Worker()
		{
			m_loop = false;
			m_workAvailable.Set();
		}

		void ThreadWorker(WorkManagerImpl& manager)
		{
			manager.PushWorker(this);
			while (true)
			{
				if (!m_workAvailable.IsSet())
					break;
				if (!m_loop)
					break;

				while (true)
				{
					while (true)
					{
						ScopedWriteLock lock(manager.m_workLock);
						if (manager.m_work.empty())
							break;
						Work work = manager.m_work.front();
						manager.m_work.pop_front();
						lock.Leave();

						work.func();
					}

					ScopedWriteLock lock1(manager.m_availableWorkersLock);
					ScopedReadLock lock2(manager.m_workLock);
					if (!manager.m_work.empty())
						continue;

					manager.PushWorkerNoLock(this);
					break;
				}
			}
		}

		Worker* m_nextWorker = nullptr;
		Worker* m_prevWorker = nullptr;
		Event m_workAvailable;
		Thread m_thread;
		bool m_loop = false;
	};

	WorkManagerImpl::WorkManagerImpl(u32 workerCount)
	{
		m_workers.resize(workerCount);
		m_activeWorkerCount = workerCount;
		for (u32 i = 0; i != workerCount; ++i)
			m_workers[i] = new Worker(*this);
	}

	WorkManagerImpl::~WorkManagerImpl()
	{
		for (u32 i = 0; i != m_workers.size(); ++i)
			delete m_workers[i];
	}


	void WorkManagerImpl::AddWork(const Function<void()>& work, u32 count, const tchar* desc)
	{
		ScopedWriteLock lock(m_workLock);
		for (u32 i = 0; i != count; ++i)
			m_work.push_back({ work });
		lock.Leave();

		ScopedWriteLock lock2(m_availableWorkersLock);
		while (count--)
		{
			Worker* worker = PopWorkerNoLock();
			if (!worker)
				break;
			worker->m_workAvailable.Set();
		}
	}

	u32 WorkManagerImpl::GetWorkerCount()
	{
		return u32(m_workers.size());
	}

	void WorkManagerImpl::PushWorker(Worker* worker)
	{
		ScopedWriteLock lock(m_availableWorkersLock);
		PushWorkerNoLock(worker);
	}

	void WorkManagerImpl::PushWorkerNoLock(Worker* worker)
	{
		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = worker;
		worker->m_prevWorker = nullptr;
		worker->m_nextWorker = m_firstAvailableWorker;
		m_firstAvailableWorker = worker;
		--m_activeWorkerCount;
	}

	void WorkManagerImpl::Wait()
	{
		while (true)
		{
			ScopedReadLock lock(m_workLock);
			bool workEmpty = m_work.empty();
			lock.Leave();
			if (workEmpty)
				break;
			Sleep(5);
		}
		while (m_activeWorkerCount)
			Sleep(5);
	}

	WorkManagerImpl::Worker* WorkManagerImpl::PopWorkerNoLock()
	{
		Worker* worker = m_firstAvailableWorker;
		if (!worker)
			return nullptr;
		m_firstAvailableWorker = worker->m_nextWorker;
		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = nullptr;
		++m_activeWorkerCount;
		return worker;
	}
}
