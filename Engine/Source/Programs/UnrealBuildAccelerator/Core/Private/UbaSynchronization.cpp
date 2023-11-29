// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSynchronization.h"
#include "UbaPlatform.h"

#if PLATFORM_WINDOWS
#define UBA_USE_WIN 1
#else
#define UBA_USE_WIN 0
#endif

#if !UBA_USE_WIN
#include <shared_mutex>
#include <mutex>
#endif

namespace uba
{

	CriticalSection::CriticalSection()
	{
		#if UBA_USE_WIN
		static_assert(sizeof(data) >= sizeof(CRITICAL_SECTION));
		InitializeCriticalSection((CRITICAL_SECTION*)&data);
		#else
		static_assert(sizeof(data) >= sizeof(std::recursive_mutex));
		new (data) std::recursive_mutex();
		#endif
	}

	CriticalSection::~CriticalSection()
	{
		#if UBA_USE_WIN
		DeleteCriticalSection((CRITICAL_SECTION*)&data);
		#else
		((std::recursive_mutex&)data).std::recursive_mutex::~recursive_mutex();
		#endif
	}

	void CriticalSection::Enter()
	{
		#if UBA_USE_WIN
		EnterCriticalSection((CRITICAL_SECTION*)&data);
		#else
		((std::recursive_mutex&)data).lock();
		#endif
	}

	void CriticalSection::Leave()
	{
		#if UBA_USE_WIN
		LeaveCriticalSection((CRITICAL_SECTION*)&data);
		#else
		((std::recursive_mutex&)data).unlock();
		#endif
	}

	ReaderWriterLock::ReaderWriterLock()
	{
		#if UBA_USE_WIN
		static_assert(sizeof(data) >= sizeof(SRWLOCK));
		InitializeSRWLock((SRWLOCK*)&data);
		#else
		static_assert(sizeof(data) >= sizeof(pthread_rwlock_t));
		pthread_rwlock_init((pthread_rwlock_t*)data, NULL);
		//static_assert(sizeof(data) >= sizeof(std::shared_mutex));
		//new (data) std::shared_mutex();
		#endif
	}

	ReaderWriterLock::~ReaderWriterLock()
	{
		#if !UBA_USE_WIN
		pthread_rwlock_destroy((pthread_rwlock_t*)data);
		//((std::shared_mutex&)data).std::shared_mutex::~shared_mutex();
		#endif
	}

	void ReaderWriterLock::EnterRead()
	{
		#if UBA_USE_WIN
		AcquireSRWLockShared((SRWLOCK*)&data);
		#else
		pthread_rwlock_rdlock((pthread_rwlock_t*)data);
		//((std::shared_mutex&)data).lock_shared();
		#endif
	}

	void ReaderWriterLock::LeaveRead()
	{
		#if UBA_USE_WIN
		ReleaseSRWLockShared((SRWLOCK*)&data);
		#else
		pthread_rwlock_unlock((pthread_rwlock_t*)data);
		//((std::shared_mutex&)data).unlock_shared();
		#endif
	}

	void ReaderWriterLock::EnterWrite()
	{
		#if UBA_USE_WIN
		AcquireSRWLockExclusive((SRWLOCK*)&data);
		#else
		pthread_rwlock_wrlock((pthread_rwlock_t*)data);
		//((std::shared_mutex&)data).lock();
		#endif
	}

	void ReaderWriterLock::LeaveWrite()
	{
		#if UBA_USE_WIN
		ReleaseSRWLockExclusive((SRWLOCK*)&data);
		#else
		pthread_rwlock_unlock((pthread_rwlock_t*)data);
		//((std::shared_mutex&)data).unlock();
		#endif
	}
}
