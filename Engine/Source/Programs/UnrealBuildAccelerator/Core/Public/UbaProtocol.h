// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	enum MessageType : u8
	{
		MessageType_Init = 1,
		MessageType_CreateFileW,
		MessageType_GetFullFileName,
		MessageType_CloseFile,
		MessageType_DeleteFileW,
		MessageType_CopyFile,
		MessageType_MoveFileW,
		MessageType_Chmod,
		MessageType_CreateDirectory,
		MessageType_ListDirectory,
		MessageType_UpdateTables,
		MessageType_CreateProcess,
		MessageType_StartProcess,
		MessageType_ExitChildProcess, // Only used for Linux since we can't wait for grand children to exit
		MessageType_CreateTempFile,
		MessageType_OpenTempFile,
		MessageType_VirtualAllocFailed,
		MessageType_Log,
		MessageType_EchoOn,
		MessageType_InputDependencies,
		MessageType_Exit,
		MessageType_FlushWrittenFiles,
		MessageType_UpdateEnvironment,
		MessageType_GetNextProcess,
		MessageType_Custom,
	};

	inline constexpr u32 CommunicationMemSize = 64*1024;

	inline constexpr u32 FileMappingTableMemSize = 16 * 1024 * 1024;
	inline constexpr u32 DirTableMemSize = 40 * 1024 * 1024;
}

// Currently only used for detoured process
#if UBA_DEBUG
#define UBA_DEBUG_LOG_ENABLED 1
#define UBA_DEBUG_VALIDATE 1
#else
#define UBA_DEBUG_LOG_ENABLED 0
#define UBA_DEBUG_VALIDATE 0
#endif
