// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputePlatform.h"
#include <assert.h>
#include <wchar.h>

#ifdef _MSC_VER
#include <Windows.h>
#undef GetEnvironmentVariable
#endif

/////////////////////////////////////////////////// 

FComputeManualResetEvent::FComputeManualResetEvent()
	: Handle(nullptr)
{
}

FComputeManualResetEvent::~FComputeManualResetEvent()
{
	Close();
}

bool FComputeManualResetEvent::Create(const wchar_t* Name)
{
	Close();

	Handle = CreateEventW(NULL, TRUE, FALSE, Name);
	return Handle != nullptr;
}

bool FComputeManualResetEvent::OpenExisting(const wchar_t* Name)
{
	Close();

	Handle = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, TRUE, Name);
	return Handle != nullptr;
}

void FComputeManualResetEvent::Close()
{
	if (Handle != nullptr)
	{
		CloseHandle(Handle);
		Handle = nullptr;
	}
}

void FComputeManualResetEvent::Set()
{
	SetEvent(Handle);
}

void FComputeManualResetEvent::Reset()
{
	ResetEvent(Handle);
}

bool FComputeManualResetEvent::Wait(int timeoutMs)
{
	DWORD WaitParam = (timeoutMs < 0) ? INFINITE : (DWORD)timeoutMs;
	return WaitForSingleObject(Handle, WaitParam) != WAIT_TIMEOUT;
}

/////////////////////////////////////////////////// 

FComputeMemoryMappedFile::FComputeMemoryMappedFile()
	: Handle(nullptr)
	, Pointer(nullptr)
{
}

FComputeMemoryMappedFile::~FComputeMemoryMappedFile()
{
	Close();
}

bool FComputeMemoryMappedFile::Create(const wchar_t* Name, long long Capacity)
{
	Close();

	LARGE_INTEGER LargeInteger;
	LargeInteger.QuadPart = Capacity;

	Handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, LargeInteger.HighPart, LargeInteger.LowPart, Name);
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
}

bool FComputeMemoryMappedFile::OpenExisting(const wchar_t* Name)
{
	Close();

	Handle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, TRUE, Name);
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
}

void FComputeMemoryMappedFile::Close()
{
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
}

void* FComputeMemoryMappedFile::GetPointer() const
{
	return Pointer;
}

/////////////////////////////////////////////////// 

bool FComputePlatform::GetEnvironmentVariable(const wchar_t* Name, wchar_t* Buffer, size_t BufferLen)
{
	int Length = GetEnvironmentVariableW(Name, Buffer, (DWORD)BufferLen);
	return Length > 0 && Length < BufferLen;
}

void FComputePlatform::CreateUniqueName(wchar_t* NameBuffer, size_t NameBufferLen)
{
	static long Counter = 0;

	DWORD Pid = GetCurrentProcessId();
	ULONGLONG TickCount = GetTickCount64();
	swprintf(NameBuffer, NameBufferLen, L"Local\\COMPUTE_%u_%llu_%lu", Pid, TickCount, AtomicIncrement(&Counter));
}

unsigned int FComputePlatform::FloorLog2(unsigned int Value)
{
	// Use BSR to return the log2 of the integer
	// return 0 if value is 0
	unsigned long BitIndex;
	return _BitScanReverse(&BitIndex, Value) ? BitIndex : 0;
}

unsigned int FComputePlatform::CountLeadingZeros(unsigned int Value)
{
	// return 32 if value is zero
	unsigned long BitIndex;
	_BitScanReverse64(&BitIndex, (unsigned long long)(Value) * 2 + 1);
	return 32 - BitIndex;
}

size_t FComputePlatform::Utf8ToWchar(const char* Source, size_t SourceLen, wchar_t* Dest, size_t DestMaxLen)
{
	size_t DecodedLen = MultiByteToWideChar(CP_UTF8, 0, Source, (int)SourceLen, Dest, (int)DestMaxLen);
	Dest[DecodedLen] = 0;
	return DecodedLen;
}

size_t FComputePlatform::WcharToUtf8(const wchar_t* Source, size_t SourceLen, char* Dest, size_t DestMaxLen)
{
	if (SourceLen == 0)
	{
		return 0;
	}

	int Result = WideCharToMultiByte(CP_UTF8, 0, Source, (int)SourceLen, Dest, (int)DestMaxLen, nullptr, nullptr);
	assert(Result > 0);

	return (size_t)Result;
}

long long FComputePlatform::AtomicRead64(const volatile long long* Ptr)
{
	return InterlockedCompareExchange64(const_cast<volatile long long*>(Ptr), 0, 0);
}

void FComputePlatform::AtomicWrite64(volatile long long* Ptr, long long Value)
{
	InterlockedExchange64(Ptr, Value);
}

long FComputePlatform::AtomicIncrement(volatile long* Ptr)
{
	return InterlockedIncrement(Ptr);
}

long FComputePlatform::AtomicDecrement(volatile long* Ptr)
{
	return InterlockedDecrement(Ptr);
}

long long FComputePlatform::AtomicAdd64(volatile long long* Ptr, long long Value)
{
	return InterlockedAdd64(Ptr, Value);
}

long FComputePlatform::AtomicAnd(volatile long* Ptr, long Value)
{
	return InterlockedAnd(Ptr, Value);
}

long long FComputePlatform::AtomicAnd64(volatile long long* Ptr, long long Value)
{
	return InterlockedAnd64(Ptr, Value);
}

long long FComputePlatform::AtomicOr64(volatile long long* Ptr, long long Value)
{
	return InterlockedOr64(Ptr, Value);
}

long long FComputePlatform::AtomicIncrement64(volatile long long* Ptr)
{
	return InterlockedIncrement64(Ptr);
}

long long FComputePlatform::AtomicExchange64(volatile long long* Ptr, long long Exchange)
{
	return InterlockedExchange64(Ptr, Exchange);
}

bool FComputePlatform::AtomicCompareExchange(volatile long* Ptr, long Exchange, long Comperand)
{
	return InterlockedCompareExchange(Ptr, Exchange, Comperand) == Comperand;
}

bool FComputePlatform::AtomicCompareExchange64(volatile long long* Ptr, long long Exchange, long long Comperand)
{
	return InterlockedCompareExchange64(Ptr, Exchange, Comperand) == Comperand;
}

