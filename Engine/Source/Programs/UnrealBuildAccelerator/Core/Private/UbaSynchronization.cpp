// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSynchronization.h"
#include "UbaPlatform.h"

namespace uba
{

	CriticalSection::CriticalSection()
	{
		#if PLATFORM_WINDOWS
		static_assert(sizeof(data) >= sizeof(CRITICAL_SECTION));
		InitializeCriticalSection((CRITICAL_SECTION*)&data);
		#else
		static_assert(sizeof(data) >= sizeof(pthread_mutex_t));
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init((pthread_mutex_t*)data, &attr);
		#endif
	}

	CriticalSection::~CriticalSection()
	{
		#if PLATFORM_WINDOWS
		DeleteCriticalSection((CRITICAL_SECTION*)&data);
		#else
		pthread_mutex_destroy((pthread_mutex_t*)data);
		#endif
	}

	void CriticalSection::Enter()
	{
		#if PLATFORM_WINDOWS
		EnterCriticalSection((CRITICAL_SECTION*)&data);
		#else
		pthread_mutex_lock((pthread_mutex_t*)data);
		#endif
	}

	void CriticalSection::Leave()
	{
		#if PLATFORM_WINDOWS
		LeaveCriticalSection((CRITICAL_SECTION*)&data);
		#else
		pthread_mutex_unlock((pthread_mutex_t*)data);
		#endif
	}

	ReaderWriterLock::ReaderWriterLock()
	{
		#if PLATFORM_WINDOWS
		static_assert(sizeof(data) >= sizeof(SRWLOCK));
		InitializeSRWLock((SRWLOCK*)&data);
		#else
		static_assert(sizeof(data) >= sizeof(pthread_rwlock_t));
		pthread_rwlock_init((pthread_rwlock_t*)data, NULL);
		#endif
	}

	ReaderWriterLock::~ReaderWriterLock()
	{
		#if !PLATFORM_WINDOWS
		pthread_rwlock_destroy((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::EnterRead()
	{
		#if PLATFORM_WINDOWS
		AcquireSRWLockShared((SRWLOCK*)&data);
		#else
		pthread_rwlock_rdlock((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::LeaveRead()
	{
		#if PLATFORM_WINDOWS
		ReleaseSRWLockShared((SRWLOCK*)&data);
		#else
		pthread_rwlock_unlock((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::EnterWrite()
	{
		#if PLATFORM_WINDOWS
		AcquireSRWLockExclusive((SRWLOCK*)&data);
		#else
		pthread_rwlock_wrlock((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::LeaveWrite()
	{
		#if PLATFORM_WINDOWS
		ReleaseSRWLockExclusive((SRWLOCK*)&data);
		#else
		pthread_rwlock_unlock((pthread_rwlock_t*)data);
		#endif
	}
}
