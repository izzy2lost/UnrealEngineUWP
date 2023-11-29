// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaThread.h"
#include "UbaPlatform.h"

namespace uba
{
	bool AlternateGroupAffinity(void* nativeThreadHandle)
	{
#if PLATFORM_WINDOWS
		static int processorGroupCount = GetActiveProcessorGroupCount();
		if (processorGroupCount == 1)
			return true;
		static Atomic<int> processorGroupCounter;
		u16 processorGroup = u16((processorGroupCounter++) % processorGroupCount);

		u32 groupProcessorCount = ::GetActiveProcessorCount(processorGroup);

		GROUP_AFFINITY groupAffinity = {};
		groupAffinity.Mask = ~0ull >> (int)(64 - groupProcessorCount);
		groupAffinity.Group = processorGroup;
		return SetThreadGroupAffinity(nativeThreadHandle, &groupAffinity, NULL);
#else
		return true;
#endif
	}

	Thread::Thread()
	:	m_handle(nullptr)
	{
	}

	Thread::Thread(Function<u32()>&& func)
	{
		Start(std::move(func));
	}

	Thread::~Thread()
	{
		if (!m_handle)
			return;
		Wait();

		#if PLATFORM_WINDOWS
		CloseHandle(m_handle);
		#endif
	}

	void Thread::Start(Function<u32()>&& f)
	{
		m_func = std::move(f);
#if PLATFORM_WINDOWS
		m_handle = CreateThread(NULL, 0, [](LPVOID p) -> DWORD { return ((Thread*)p)->m_func(); }, this, 0, NULL);
		AlternateGroupAffinity(m_handle);
#else
		m_finished.Create(true);
		static_assert(sizeof(pthread_t) <= sizeof(m_handle), "");
		auto& pth = *(pthread_t*)&m_handle;
		int err = pthread_create(&pth, NULL, [](void* p) -> void*
			{
				auto& t = *(Thread*)p;
				int res = t.m_func();
				t.m_finished.Set();
				return (void*)(uintptr_t)res;
			}, this);
		UBA_ASSERT(err == 0); (void)err;
#endif
	}

	bool Thread::Wait(u32 milliseconds, Event* wakeupEvent)
	{
		if (!m_handle)
			return true;

#if PLATFORM_WINDOWS // Optimization, not needed in initial implementation
		if (wakeupEvent)
		{
			HANDLE h[] = { m_handle, wakeupEvent->GetHandle() };
			DWORD res = WaitForMultipleObjects(2, h, false, milliseconds);
			if (res == WAIT_OBJECT_0 + 1 || res == WAIT_TIMEOUT)
				return false;
		}
		else
		{
			if (WaitForSingleObject(m_handle, milliseconds) == WAIT_TIMEOUT)
				return false;
		}
		CloseHandle(m_handle);
#else
		if (!m_finished.IsSet(milliseconds))
			return false;
		int* ptr = 0;
		int res = pthread_join(*(pthread_t*)&m_handle, (void**)&ptr);
		UBA_ASSERT(res == 0);
#endif
		m_handle = nullptr;
		return true;
	}
}
