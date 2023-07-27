// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

// Defines for the current platform
#ifdef _MSC_VER
	#define UE_COMPUTE_PLATFORM_WINDOWS 1
	#define UE_COMPUTE_PLATFORM_MAC 0
	#define UE_COMPUTE_PLATFORM_LINUX 0
#elif defined(__APPLE__)
	#define UE_COMPUTE_PLATFORM_WINDOWS 0
	#define UE_COMPUTE_PLATFORM_MAC 1
	#define UE_COMPUTE_PLATFORM_LINUX 0
#else
	#define UE_COMPUTE_PLATFORM_WINDOWS 0
	#define UE_COMPUTE_PLATFORM_MAC 0
	#define UE_COMPUTE_PLATFORM_LINUX 1
#endif

//
// IPC manually-reset event object
//
class FComputeEvent
{
public:
	FComputeEvent();
	~FComputeEvent();

	// Creates a new event with the given name
	bool Create(const char* Name);

	// Opens an existing event created elsewhere
	bool OpenExisting(const char* Name);

	// Close the event and release its resources
	void Close();

	// Signal the event, releasing any waiters
	void Signal();

	// Wait for the event to be signalled or timeout. Pass -1 for timeoutMs to wait infinitely.
	bool Wait(int timeoutMs);

private:
	void* Handle;
};

//
// IPC memory mapped file
//
class FComputeMemoryMappedFile
{
public:
	FComputeMemoryMappedFile();
	~FComputeMemoryMappedFile();

	// Creates a new memory mapped file with the given capacity
	bool Create(const char* Name, long long Capacity);

	// Opens an existing memory mapped file
	bool OpenExisting(const char* Name);

	// Close the memory mapped file handle
	void Close();

	// Gets a pointer to the mapped memory
	void* GetPointer() const;

private:
	void* Handle;
	void* Pointer;
	long long MappedSize;
	char* OwnerName;
};

//
// Platform-specific utility functions
//
struct FComputePlatform
{
	//
	// General
	//

	// Reads an environment variable
	static bool GetEnvironmentVariable(const char* Name, char* Buffer, size_t BufferLen);

	// Creates a unique object name
	static void CreateUniqueName(char* NameBuffer, size_t NameBufferLen);

	//
	// Math
	//

	// Find the log2 of the given value, returning 0 if the value is zero.
	static unsigned int FloorLog2(unsigned int Value);

	// Count the number of leading zeros in the given value.
	static unsigned int CountLeadingZeros(unsigned int Value);

	//
	// Strings
	//

	// Copy a string from one buffer to another, not exceeding the destination buffer size
	static void Strcpy(char* Dest, size_t DestLen, const char* Source);

	// Perform a case-insensitive comparison of two strings
	static int Stricmp(const char* A, const char* B);

	//
	// Atomics
	//

	// Reads a 64-bit value from the given memory location 
	static long long AtomicRead64(const volatile long long* Ptr);

	// Writes a 64-bit value to the given memory location
	static void AtomicWrite64(volatile long long* Ptr, long long Value);

	// Increments an integer at the given memory location and returns the incremented value.
	static long AtomicIncrement(volatile long* Ptr);

	// Increments an integer at the given memory location and returns the incremented value.
	static long long AtomicIncrement64(volatile long long* Ptr);

	// Decrements an integer at the given memory location and returns the decremented value.
	static long AtomicDecrement(volatile long* Ptr);

	// Adds a value to the given memory location, and returns the resulting value.
	static long long AtomicAdd64(volatile long long* Ptr, long long Value);

	// ANDs a value with a given memory location, and returns the contents of the memory location BEFORE the operation.
	static long AtomicAnd(volatile long* Ptr, long Value);

	// ANDs a value with a given memory location, and returns the contents of the memory location BEFORE the operation.
	static long long AtomicAnd64(volatile long long* Ptr, long long Value);

	// ORs a value with a given memory location, and returns the contents of the memory location BEFORE the operation.
	static long long AtomicOr64(volatile long long* Ptr, long long Value);

	// Sets the contents of a pointer to the given value, and returns the original value in that memory location.
	static long long AtomicExchange64(volatile long long* Ptr, long long Exchange);

	// Compares the contents of a memory location to a value, and exchanges it for another value if they are equal. Returns true if the operation succeeded.
	static bool AtomicCompareExchange(volatile long* Ptr, long Exchange, long Comperand);

	// Compares the contents of a memory location to a value, and exchanges it for another value if they are equal. Returns true if the operation succeeded.
	static bool AtomicCompareExchange64(volatile long long* Ptr, long long Exchange, long long Comperand);
};
