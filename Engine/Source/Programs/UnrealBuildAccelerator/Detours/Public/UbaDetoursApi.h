// Copyright Epic Games, Inc. All Rights Reserved.

#if UBA_IS_DETOURED_INCLUDE
#define UBA_DETOURED_API __declspec(dllexport)
#else
#define UBA_DETOURED_API __declspec(dllimport)
#endif

extern "C"
{
	// Send custom message from detoured process all the way out to callback registered with RegisterCustomService
	// Handles remote processes as well and network the message
	UBA_DETOURED_API unsigned int UbaSendCustomMessage(const void* send, unsigned int sendSize, void* recv, unsigned int recvCapacity);

	// Make sure all written files are flushed to destination (lots of files are kept in memory until process ends otherwise)
	UBA_DETOURED_API bool UbaFlushWrittenFiles();

	// Update environment. Making sure caches are up-to-date in detoured process.
	// reason ends up in the visualizer for the next block of process
	UBA_DETOURED_API bool UbaUpdateEnvironment(const wchar_t* reason);

	// Returns true if process is running on a remote machine
	UBA_DETOURED_API bool UbaRunningRemote();

	// Using custom message inside UbaScheduler so can't be combined with RegisterCustomService.
	UBA_DETOURED_API bool UbaRequestNextProcess(wchar_t* outArguments, unsigned int outArgumentsCapacity);
}


// Above functions can be found and used using code looking like this:

#if 0
// Windows
static HMODULE UbaDetoursModule = GetModuleHandleW(L"UbaDetours.dll");
if (UbaDetoursModule)
{
	using UbaFlushWrittenFilesFunc = bool();
	using UbaRequestNextProcessFunc = bool(TCHAR* outArguments, uint32 outArgumentsCapacity);
	using UbaUpdateEnvironmentFunc = bool(const wchar_t* reason);

	static UbaFlushWrittenFilesFunc* FlushWrittenFiles = (UbaFlushWrittenFilesFunc*)(void*)GetProcAddress(UbaDetoursModule, "UbaFlushWrittenFiles");
	static UbaRequestNextProcessFunc* RequestNextProcess = (UbaRequestNextProcessFunc*)(void*)GetProcAddress(UbaDetoursModule, "UbaRequestNextProcess");
	static UbaUpdateEnvironmentFunc* UpdateEnvironment = (UbaUpdateEnvironmentFunc*)(void*)GetProcAddress(UbaDetoursModule, "UbaUpdateEnvironment");

	// ...
}

// Linux/Mac not implemented yet
...
#endif