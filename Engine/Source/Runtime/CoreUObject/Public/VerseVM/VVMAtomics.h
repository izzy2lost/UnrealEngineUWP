// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"
#include <atomic>

namespace Verse
{

//// Just a compiler fence. Has no effect on the hardware, but tells the compiler
//// not to move things around this call. Should not affect the compiler's ability
//// to do things like register allocation and code motion over pure operations.
inline void compilerFence()
{
#if PLATFORM_WINDOWS
	_ReadWriteBarrier();
#else
	asm volatile("" ::
					 : "memory");
#endif
}

#if PLATFORM_CPU_ARM_FAMILY

//// Full memory fence. No accesses will float above this, and no accesses will sink below it.
inline void arm_dmb()
{
	asm volatile("dmb ish" ::
					 : "memory");
}

// Like the above, but only affects stores.
inline void arm_dmb_st()
{
	asm volatile("dmb ishst" ::
					 : "memory");
}

inline void arm_isb()
{
	asm volatile("isb" ::
					 : "memory");
}

inline void loadLoadFence()
{
	arm_dmb();
}
inline void loadStoreFence()
{
	arm_dmb();
}
inline void storeLoadFence()
{
	arm_dmb();
}
inline void storeStoreFence()
{
	arm_dmb_st();
}
inline void crossModifyingCodeFence()
{
	arm_isb();
}

#elif PLATFORM_CPU_X86_FAMILY

inline void x86_ortop()
{
#if PLATFORM_WINDOWS
	FGenericPlatformMisc::MemoryBarrier();
#elif PLATFORM_64BITS
	asm volatile("lock; orl $0, (%%rsp)" ::
					 : "memory");
#else
	asm volatile("lock; orl $0, (%%esp)" ::
					 : "memory");
#endif
}

inline void x86_cpuid()
{
#if PLATFORM_WINDOWS
	int info[4];
	__cpuid(info, 0);
#else
	intptr_t a = 0, b, c, d;
	asm volatile(
		"cpuid"
		: "+a"(a), "=b"(b), "=c"(c), "=d"(d)
		:
		: "memory");
#endif
}

inline void loadLoadFence()
{
	compilerFence();
}
inline void loadStoreFence()
{
	compilerFence();
}
inline void storeLoadFence()
{
	x86_ortop();
}
inline void storeStoreFence()
{
	compilerFence();
}
inline void crossModifyingCodeFence()
{
	x86_cpuid();
}

#else

inline void loadLoadFence()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
}
inline void loadStoreFence()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
}
inline void storeLoadFence()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
}
inline void storeStoreFence()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
}
inline void crossModifyingCodeFence()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
} // Probably not strong enough.

#endif

} // namespace Verse
#endif // WITH_VERSE_VM
