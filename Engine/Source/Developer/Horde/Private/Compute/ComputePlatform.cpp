// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/ComputePlatform.h"
#include <assert.h>
#include <wchar.h>
#include <bit>
#include <algorithm>
#include <iostream>

#if UE_COMPUTE_PLATFORM_WINDOWS
	#include <Windows.h>
	#undef min
	#undef max
	#undef GetEnvironmentVariable
	#undef SendMessage
#else
	#include <semaphore.h>
	#include <unistd.h>
	#include <atomic>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <time.h>
	#include <fcntl.h>

	struct FFormatErrno { };

	static std::ostream &operator<<(std::ostream &os, const FFormatErrno &)
	{
		os << "(errno=" << errno << ": " << strerror(errno) << ")";
		return os;
	}
#endif

///////////////////////////////////////////////////

FComputeEvent::FComputeEvent()
	: Handle(nullptr)
{
}

FComputeEvent::~FComputeEvent()
{
	Close();
}

bool FComputeEvent::Create(const char* Name)
{
	Close();

#if UE_COMPUTE_PLATFORM_WINDOWS
	Handle = CreateEventA(NULL, FALSE, FALSE, Name);
	return Handle != nullptr;
#else
	sem_t* Value = sem_open(Name, O_CREAT | O_EXCL, 0666, 1);
	if(Value != SEM_FAILED)
	{
		Handle = Value;
		return true;
	}
	return false;
#endif
}

bool FComputeEvent::OpenExisting(const char* Name)
{
	Close();

#if UE_COMPUTE_PLATFORM_WINDOWS
	Handle = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, TRUE, Name);
	return Handle != nullptr;
#else
	sem_t* Value = sem_open(Name, 0);
	if(Value != SEM_FAILED)
	{
		Handle = Value;
		return true;
	}
	return false;
#endif
}

void FComputeEvent::Close()
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	if (Handle != nullptr)
	{
		CloseHandle(Handle);
		Handle = nullptr;
	}
#else
	if (Handle != nullptr)
	{
		sem_close((sem_t*)Handle);
		Handle = nullptr;
	}
#endif
}

void FComputeEvent::Signal()
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	SetEvent(Handle);
#else
	sem_post((sem_t*)Handle);
#endif
}

bool FComputeEvent::Wait(int timeoutMs)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	DWORD WaitParam = (timeoutMs < 0) ? INFINITE : (DWORD)timeoutMs;
	return WaitForSingleObject(Handle, WaitParam) != WAIT_TIMEOUT;
#else
	if (timeoutMs == -1)
	{
		return sem_wait((sem_t*)Handle) == 0;
	}

	if (sem_trywait((sem_t*)Handle) == 0)
	{
		return true;
	}

	if(timeoutMs == 0)
	{
		return false;
	}
		
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
	{
		UE_COMPUTE_ASSERT(false);
		return false;
	}
	
	const long long NsPerSec = 1'000'000'000;
	const long long NsPerMs = NsPerSec / 1000;
	
	long long newNs = (long long)ts.tv_nsec + (timeoutMs * NsPerMs);
	ts.tv_nsec = newNs % NsPerSec;
	ts.tv_sec += newNs / NsPerSec;
	
#if UE_COMPUTE_PLATFORM_MAC
	for(;;)
	{
		if (sem_trywait((sem_t*)Handle) == 0)
		{
			return true;
		}
		
		struct timespec currentTs;
		if (clock_gettime(CLOCK_REALTIME, &currentTs) != 0 || currentTs.tv_sec > ts.tv_sec || (currentTs.tv_sec == ts.tv_sec && currentTs.tv_nsec > ts.tv_nsec))
		{
			return false;
		}
		
		struct timespec sleepTs = { 0, };
		sleepTs.tv_nsec = 100 * NsPerMs;
		
		nanosleep(&sleepTs, nullptr);
	}
#else
	return sem_timedwait((sem_t*)Handle, &ts) == 0;
#endif
#endif
}

/////////////////////////////////////////////////// 

FComputeMemoryMappedFile::FComputeMemoryMappedFile()
	: Handle(nullptr)
	, Pointer(nullptr)
	, MappedSize(0)
	, OwnerName(nullptr)
{
}

FComputeMemoryMappedFile::~FComputeMemoryMappedFile()
{
	Close();
}

bool FComputeMemoryMappedFile::Create(const char* Name, long long Capacity)
{
	Close();

#if UE_COMPUTE_PLATFORM_WINDOWS
	LARGE_INTEGER LargeInteger;
	LargeInteger.QuadPart = Capacity;

	Handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, LargeInteger.HighPart, LargeInteger.LowPart, Name);
	if (Handle == nullptr)
	{
		return false;
	}

	Pointer = MapViewOfFile(Handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (Pointer == nullptr)
	{
		return false;
	}

	return true;
#else
	int Fd = shm_open(Name, O_CREAT | O_EXCL | O_RDWR, 0666);
	if(Fd < 0)
	{
		std::cerr << "Unable to create shared memory object '" << Name << "' " << FFormatErrno() << std::endl;
		return false;
	}

	Handle = (void*)(size_t)Fd;
	MappedSize = Capacity;
	OwnerName = strdup(Name);

	if(ftruncate(Fd, MappedSize) < 0)
	{
		std::cerr << "Unable to update size of shared memory object '" << Name << "' to " << MappedSize << " " << FFormatErrno() << std::endl;
		return false;
	}

	Pointer = mmap(nullptr, MappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, Fd, 0);
	if(Pointer == MAP_FAILED)
	{
		std::cerr << "Unable to map shared memory object '" << Name << " " << FFormatErrno() << std::endl;
		return false;
	}
	
	return true;
#endif
}

bool FComputeMemoryMappedFile::OpenExisting(const char* Name)
{
	Close();

#if UE_COMPUTE_PLATFORM_WINDOWS
	Handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, TRUE, Name);
	if (Handle == nullptr)
	{
		return false;
	}

	Pointer = MapViewOfFile(Handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (Pointer == nullptr)
	{
		return false;
	}

	return true;
#else
	int Fd = shm_open(Name, O_RDWR, 0666);
	if(Fd < 0)
	{
		std::cerr << "Unable to open shared memory object '" << Name << "' " << FFormatErrno() << std::endl;
		return false;
	}

	Handle = (void*)(size_t)Fd;

	struct stat st;
	fstat(Fd, &st);
	MappedSize = st.st_size;

	Pointer = mmap(nullptr, MappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, Fd, 0);
	if(Pointer == MAP_FAILED)
	{
		std::cerr << "Unable to map shared memory object '" << Name << " " << FFormatErrno() << std::endl;
		return false;
	}
	
	return true;
#endif
}

void FComputeMemoryMappedFile::Close()
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	if (Pointer != nullptr)
	{
		UnmapViewOfFile(Pointer);
		Pointer = nullptr;
	}

	if (Handle != nullptr)
	{
		CloseHandle(Handle);
		Handle = nullptr;
	}
#else
	if(OwnerName != nullptr)
	{
		shm_unlink(OwnerName);
		OwnerName = nullptr;
	}
	
	if(Pointer != nullptr)
	{
		munmap(Pointer, MappedSize);
		Pointer = nullptr;
	}

	int Fd = (int)(size_t)Handle;
	if(Fd >= 0)
	{
		close(Fd);
		Handle = nullptr;
	}
#endif
}

void* FComputeMemoryMappedFile::GetPointer() const
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return Pointer;
#else
	return (unsigned char*)Pointer + 16;
#endif
}

/////////////////////////////////////////////////// 

void FComputePlatform::AssertFailed(const char* Expr, const char* File, int Line)
{
	std::cerr << "Assertion failed: '" << Expr << std::endl;
	std::cerr << "  at " << File << "(" << Line << ")" << std::endl;
#if UE_COMPUTE_PLATFORM_WINDOWS
	DebugBreak();
#endif
	exit(1);
}

bool FComputePlatform::GetEnvironmentVariable(const char* Name, char* Buffer, size_t BufferLen)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	int Length = GetEnvironmentVariableA(Name, Buffer, (DWORD)BufferLen);
	return Length > 0 && Length < BufferLen;
#else
	char* Value = getenv(Name);
	if(Value != nullptr)
	{
		FComputePlatform::Strcpy(Buffer, BufferLen, Value);
		return true;
	}
	return false;
#endif
}

void FComputePlatform::CreateUniqueName(char* NameBuffer, size_t NameBufferLen)
{
	static long Counter = 0;

#if UE_COMPUTE_PLATFORM_WINDOWS
	DWORD Pid = GetCurrentProcessId();
	ULONGLONG TickCount = GetTickCount64();
	snprintf(NameBuffer, NameBufferLen, "Local\\COMPUTE_%lu_%llu_%lu", Pid, TickCount, (unsigned long)AtomicIncrement(&Counter));
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	snprintf(NameBuffer, NameBufferLen, "/UEC_%u%zu%zu_%lu", getpid(), (size_t)ts.tv_sec, (size_t)ts.tv_nsec, AtomicIncrement(&Counter));
#endif
}

unsigned int FComputePlatform::FloorLog2(unsigned int Value)
{
	return std::max<unsigned int>(0, 31 - std::countl_zero(Value));
}

unsigned int FComputePlatform::CountLeadingZeros(unsigned int Value)
{
	return std::countl_zero(Value);
}

void FComputePlatform::Strcpy(char* Dest, size_t DestLen, const char* Source)
{
	size_t Length = std::min(strlen(Source), DestLen - 1);
	memcpy(Dest, Source, Length);
	Dest[Length] = 0;
}

int FComputePlatform::Stricmp(const char* A, const char* B)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return _stricmp(A, B);
#else
	return strcasecmp(A, B);
#endif
}

long long FComputePlatform::AtomicRead64(const volatile long long* Ptr)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedCompareExchange64(const_cast<volatile long long*>(Ptr), 0, 0);
#else
	return std::atomic_load((std::atomic<long long>*)Ptr);
#endif
}

void FComputePlatform::AtomicWrite64(volatile long long* Ptr, long long Value)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	InterlockedExchange64(Ptr, Value);
#else
	return std::atomic_store((std::atomic<long long>*)Ptr, Value);
#endif
}

long FComputePlatform::AtomicIncrement(volatile long* Ptr)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedIncrement(Ptr);
#else
	return std::atomic_fetch_add((std::atomic<long>*)Ptr, 1) + 1;
#endif
}

long FComputePlatform::AtomicDecrement(volatile long* Ptr)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedDecrement(Ptr);
#else
	return std::atomic_fetch_add((std::atomic<long>*)Ptr, -1) - 1;
#endif
}

long long FComputePlatform::AtomicAdd64(volatile long long* Ptr, long long Value)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedAdd64(Ptr, Value);
#else
	return std::atomic_fetch_add((std::atomic<long long>*)Ptr, Value) + Value;
#endif
}

long FComputePlatform::AtomicAnd(volatile long* Ptr, long Value)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedAnd(Ptr, Value);
#else
	return std::atomic_fetch_and((std::atomic<long>*)Ptr, Value);
#endif
}

long long FComputePlatform::AtomicAnd64(volatile long long* Ptr, long long Value)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedAnd64(Ptr, Value);
#else
	return std::atomic_fetch_and((std::atomic<long long>*)Ptr, Value);
#endif
}

long long FComputePlatform::AtomicOr64(volatile long long* Ptr, long long Value)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedOr64(Ptr, Value);
#else
	return std::atomic_fetch_or((std::atomic<long long>*)Ptr, Value);
#endif
}

long long FComputePlatform::AtomicIncrement64(volatile long long* Ptr)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedIncrement64(Ptr);
#else
	return std::atomic_fetch_add((std::atomic<long long>*)Ptr, 1) + 1;
#endif
}

long long FComputePlatform::AtomicExchange64(volatile long long* Ptr, long long Exchange)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedExchange64(Ptr, Exchange);
#else
	return std::atomic_exchange((std::atomic<long long>*)Ptr, Exchange);
#endif
}

bool FComputePlatform::AtomicCompareExchange(volatile long* Ptr, long Exchange, long Comperand)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedCompareExchange(Ptr, Exchange, Comperand) == Comperand;
#else
	return std::atomic_compare_exchange_strong((std::atomic<long>*)Ptr, &Comperand, Exchange);
#endif
}

bool FComputePlatform::AtomicCompareExchange64(volatile long long* Ptr, long long Exchange, long long Comperand)
{
#if UE_COMPUTE_PLATFORM_WINDOWS
	return InterlockedCompareExchange64(Ptr, Exchange, Comperand) == Comperand;
#else
	return std::atomic_compare_exchange_strong((std::atomic<long long>*)Ptr, &Comperand, Exchange);
#endif
}

