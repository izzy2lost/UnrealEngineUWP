// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

//
// IPC manually-reset event object
//
class FComputeEvent
{
public:
	FComputeEvent();
	~FComputeEvent();

	// Creates a new event with the given name
	HORDE_API bool Create(const char* Name);

	// Opens an existing event created elsewhere
	HORDE_API bool OpenExisting(const char* Name);

	// Close the event and release its resources
	HORDE_API void Close();

	// Signal the event, releasing any waiters
	HORDE_API void Signal();

	// Wait for the event to be signalled or timeout. Pass -1 for timeoutMs to wait infinitely.
	HORDE_API bool Wait(int timeoutMs);

private:
	void* Handle;
};

//
// IPC memory mapped file
//
class FComputeMemoryMappedFile
{
public:
	HORDE_API FComputeMemoryMappedFile();
	HORDE_API ~FComputeMemoryMappedFile();

	// Creates a new memory mapped file with the given capacity
	HORDE_API bool Create(const char* Name, long long Capacity);

	// Opens an existing memory mapped file
	HORDE_API bool OpenExisting(const char* Name);

	// Close the memory mapped file handle
	HORDE_API void Close();

	// Gets a pointer to the mapped memory
	HORDE_API void* GetPointer() const;

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

	// Signal that an assertion has failed
	HORDE_API static void AssertFailed(const char* Expr, const char* File, int Line);
	
	// Reads an environment variable
	HORDE_API static bool GetEnvironmentVariable(const char* Name, char* Buffer, size_t BufferLen);

	// Creates a unique object name
	HORDE_API static void CreateUniqueName(char* NameBuffer, size_t NameBufferLen);

	//
	// Math
	//

	// Find the log2 of the given value, returning 0 if the value is zero.
	HORDE_API static unsigned int FloorLog2(unsigned int Value);

	// Count the number of leading zeros in the given value.
	HORDE_API static unsigned int CountLeadingZeros(unsigned int Value);

	//
	// Strings
	//

	// Copy a string from one buffer to another, not exceeding the destination buffer size
	HORDE_API static void Strcpy(char* Dest, size_t DestLen, const char* Source);

	// Perform a case-insensitive comparison of two strings
	HORDE_API static int Stricmp(const char* A, const char* B);
};
