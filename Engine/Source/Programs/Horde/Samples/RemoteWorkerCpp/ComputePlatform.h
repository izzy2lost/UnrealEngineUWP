// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//
// IPC manually-reset event object
//
class FComputeManualResetEvent
{
public:
	FComputeManualResetEvent();
	~FComputeManualResetEvent();

	// Creates a new event with the given name
	bool Create(const wchar_t* Name);

	// Opens an existing event created elsewhere
	bool OpenExisting(const wchar_t* Name);

	// Close the event and release its resources
	void Close();

	// Signal the event, releasing any waiters
	void Set();

	// Reset the event after a call to Set()
	void Reset();

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
	bool Create(const wchar_t* Name, long long Capacity);

	// Opens an existing memory mapped file
	bool OpenExisting(const wchar_t* Name);

	// Close the memory mapped file handle
	void Close();

	// Gets a pointer to the mapped memory
	void* GetPointer() const;

private:
	void* Handle;
	void* Pointer;
};

//
// Platform-specific utility functions
//
struct FComputePlatform
{
	//
	// General
	//
	// 
	// Reads an environment variable
	static bool GetEnvironmentVariable(const wchar_t* Name, wchar_t* Buffer, size_t BufferLen);

	// Creates a unique object name
	static void CreateUniqueName(wchar_t* NameBuffer, size_t NameBufferLen);

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

	// Translate a UTF8 string to a wchar_t string. Returns the length of the converted string in characters, even if the supplied buffer is not large enough to hold it.
	static size_t Utf8ToWchar(const char* Source, size_t SourceLen, wchar_t* Dest, size_t DestMaxLen);

	// Translate a wchar_t string to a UTF8 string. Returns the length of the converted string in characters, even if the supplied buffer is not large enough to hold it.
	static size_t WcharToUtf8(const wchar_t* Source, size_t SourceLen, char* Dest, size_t DestMaxLen);

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
	static long long AtomicAnd64(volatile long long* Ptr, long long Value);

	// ORs a value with a given memory location, and returns the contents of the memory location BEFORE the operation.
	static long long AtomicOr64(volatile long long* Ptr, long long Value);

	// Sets the contents of a pointer to the given value, and returns the original value in that memory location.
	static long long AtomicExchange64(volatile long long* Ptr, long long Exchange);

	// Compares the contents of a memory location to a value, and exchanges it for another value if they are equal. Returns true if the operation succeeded.
	static bool AtomicCompareExchange64(volatile long long* Ptr, long long Exchange, long long Comperand);
};
