// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"
#include <atomic>
#include <utility>

namespace uba
{
	template<typename Type>
	using Atomic = std::atomic<Type>;

	struct AtomicU64 : Atomic<u64>
	{
		AtomicU64(u64 initialValue = 0) : Atomic<u64>(initialValue) {}
		AtomicU64(AtomicU64&& o) noexcept : Atomic<u64>(o.load()) {}
		void operator=(u64 o) { store(o); }
		void operator=(const AtomicU64& o) { store(o); }
	};

	class CriticalSection
	{
	public:
		CriticalSection();
		~CriticalSection();

		void Enter();
		void Leave();

		template<class Functor> auto Scoped(const Functor& f);

	private:
		#if PLATFORM_WINDOWS
		u64 data[5];
		#else
		u64 data[10];
		#endif

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator=(const CriticalSection&) = delete;
	};

	class ScopedCriticalSection 
	{
	public:
		ScopedCriticalSection(CriticalSection& cs) : m_cs(cs), m_active(true) { cs.Enter(); }
		~ScopedCriticalSection() { Leave(); }
		void Enter() { if (m_active) return; m_cs.Enter(); m_active = true; }
		void Leave() { if (!m_active) return; m_cs.Leave(); m_active = false; }
	private:
		CriticalSection& m_cs;
		bool m_active;
	};

	template<class Functor> auto CriticalSection::Scoped(const Functor& f) { ScopedCriticalSection c(*this); return f(); }

	class ReaderWriterLock
	{
	public:
		ReaderWriterLock();
		~ReaderWriterLock();

		void EnterRead();
		void LeaveRead();

		void EnterWrite();
		void LeaveWrite();

		template<class Functor> auto ScopedRead(const Functor& f);
		template<class Functor> auto ScopedWrite(const Functor& f);

	private:

		#if PLATFORM_WINDOWS
		u64 data[1];
		#elif PLATFORM_LINUX
		u64 data[7];
		#else
		u64 data[25];
		#endif

		ReaderWriterLock(const ReaderWriterLock&) = delete;
		ReaderWriterLock& operator=(const ReaderWriterLock&) = delete;
	};

	class ScopedReadLock
	{
	public:
		ScopedReadLock(ReaderWriterLock& lock) : m_lock(lock) { lock.EnterRead(); }
		~ScopedReadLock() { Leave(); }
		inline void Leave() { if (!m_active) return; m_active = false; m_lock.LeaveRead(); }

		ReaderWriterLock& m_lock;
		bool m_active = true;
	};

	class ScopedWriteLock
	{
	public:
		ScopedWriteLock(ReaderWriterLock& lock) : m_lock(lock) { lock.EnterWrite(); }
		~ScopedWriteLock() { Leave(); }
		inline void Enter() { if (m_active) return; m_active = true; m_lock.EnterWrite(); }
		inline void Leave() { if (!m_active) return; m_active = false; m_lock.LeaveWrite(); }

		ReaderWriterLock& m_lock;
		bool m_active = true;
	};

	template<class Functor> auto ReaderWriterLock::ScopedRead(const Functor& f) { ScopedReadLock l(*this); return f(); }
	template<class Functor> auto ReaderWriterLock::ScopedWrite(const Functor& f) { ScopedWriteLock l(*this); return f(); }


	template<typename Lambda>
	struct ScopeGuard
	{
		void Cancel() { m_called = true; }
		auto Execute() { m_called = true; return m_lambda(); }

		ScopeGuard(Lambda lambda) : m_lambda(lambda) {}
		ScopeGuard(ScopeGuard&& o) { m_lambda = std::move(o.m_lambda); o.m_called = true; }
		ScopeGuard() = delete;
		ScopeGuard(const ScopeGuard& o) = delete;
		void operator=(const ScopeGuard&) = delete;
		void operator=(ScopeGuard&&) = delete;
		~ScopeGuard() { if (!m_called) m_lambda(); }
	private:
		Lambda m_lambda;
		bool m_called = false;
	};
	template<typename Lambda>
	ScopeGuard<Lambda> MakeGuard(Lambda&& lambda) { return ScopeGuard<Lambda>(std::move(lambda)); }
}
