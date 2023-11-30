// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define Local_GetLongPathNameW uba::GetLongPathNameW

#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaPathUtils.h"
#include "UbaPlatform.h"
#include "UbaEvent.h"
#include "UbaFile.h"
#include "UbaLogger.h"
#include "UbaThread.h"
#include "UbaTimer.h"
#include "UbaDirectoryIterator.h"

namespace uba
{
	bool TestTime(Logger& logger, const StringBufferBase& rootDir)
	{
		#if 0
		LoggerWithWriter consoleLogger(g_consoleLogWriter); (void)consoleLogger;
		u64 time1 = GetSystemTimeUs();
		Sleep(1000);
		u64 time2 = GetSystemTimeUs();
		u64 ms = (time2 - time1) / 1000;
		consoleLogger.Info(TC("Slept ms: %llu"), ms);
		
		time1 = GetTime();
		Sleep(1000);
		time2 = GetTime();
		ms = (time2 * 1000 / GetFrequency()) - (time1 * 1000 / GetFrequency());
		consoleLogger.Info(TC("Slept ms: %llu"), ms);
		#endif

		return true;
	}

	bool TestEvents(Logger& logger, const StringBufferBase& rootDir)
	{
		Event ev(true);
		Thread t([&]()
			{
				Sleep(500);
				//logger.Info(TC("Setting event"));
				ev.Set();
				Sleep(500);
				return true;
			});

		if (ev.IsSet(0))
			return false;

		//logger.Info(TC("Waiting for event"));
		if (!ev.IsSet(10000))
			return false;
		//logger.Info(TC("Event was set"));

		if (t.Wait(0))
			return false;

		if (!t.Wait(2000))
			return false;
		return true;
	}

	bool TestPaths(Logger& logger, const StringBufferBase& rootDir)
	{
		#if PLATFORM_WINDOWS
		const tchar* workingDir = TC("e:\\dev\\");
		const tchar* filename = TC("\"e:\\temp\"");
		tchar buffer[1024];
		u32 lengthResult;
		if (!FixPath2(filename, workingDir, TStrlen(workingDir), buffer, &lengthResult))
			return logger.Error(TC("FixPath2 (1) failed"));
		#endif
		return true;
	}

	bool TestFiles(Logger& logger, const StringBufferBase& rootDir)
	{
		FileAccessor fileHandle(logger, TC("UbaTestFile"));
		if (!fileHandle.CreateWrite())
			return logger.Error(TC("Failed to create file for write"));

		u8 byte = 'H';
		if (!fileHandle.Write(&byte, 1))
			return false;

		if (!fileHandle.Close())
			return false;

		FileHandle fileHandle2;
		if (!OpenFileSequentialRead(logger, TC("UbaTestFile"), fileHandle2))
			return logger.Error(TC("Failed to create file for read"));

		u64 writeTime = 0;
		if (!GetFileLastWriteTime(writeTime, fileHandle2))
			return logger.Error(TC("Failed to get last written time"));

		u64 systemTime = GetSystemTimeAsFileTime();
		if (GetFileTimeAsSeconds(systemTime)+1 - GetFileTimeAsSeconds(writeTime) > 3)
			return logger.Error(TC("system time or last written time is wrong (system: %llu, write: %llu)"), systemTime, writeTime);


		u8 byte2 = 0;
		if (!ReadFile(logger, TC("UbaTestFile"), fileHandle2, &byte2, 1))
			return false;

		if (!CloseFile(TC("UbaTestFile"), fileHandle2))
			return false;

		FileHandle fileHandle3;
		if (!OpenFileSequentialRead(logger, TC("NonExistingFile"), fileHandle3, false))
			return logger.Error(TC("OpenFileSequentialRead failed with non existing file"));
		if (fileHandle3 != InvalidFileHandle)
			return logger.Error(TC("OpenFileSequentialRead found file that doesn't exist"));

		if (RemoveDirectoryW(TC("TestDir")))
			return logger.Error(TC("Did not fail to remove non-existing TestDir (or were things not cleaned before test)"));
		else if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return logger.Error(TC("GetLastError did not return correct error failing to remove non-existing directory TestDir"));

		if (!CreateDirectoryW(TC("TestDir")))
			return logger.Error(TC("Failed to create dir"));

		FileHandle fileHandle4;
		if (OpenFileSequentialRead(logger, TC("TestDir"), fileHandle4))
			return logger.Error(TC("This should return fail"));

		if (!RemoveDirectoryW(TC("TestDir")))
			return logger.Error(TC("Fail to remove TestDir"));

		u64 size = 0;
		if (!FileExists(logger, TC("UbaTestFile"), &size) || size != 1)
			return logger.Error(TC("UbaTestFile not found"));

		DeleteFileW(TC("UbaTestFile2"));

		if (DeleteFileW(TC("UbaTestFile2")))
			return logger.Error(TC("Did not fail to delete non-existing UbaTestFile2 (or were things not cleaned before test)"));
		else if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return logger.Error(TC("GetLastError did not return correct error failing to delete non-existing file UbaTestFile2"));

		if (!CreateHardLinkW(TC("UbaTestFile2"), TC("UbaTestFile")))
			return logger.Error(TC("Failed to create hardlink from UbaTestFile to UbaTestFile2"));

		if (!DeleteFileW(TC("UbaTestFile")))
			return logger.Error(TC("Failed to delete UbaTestFile"));

		if (FileExists(logger, TC("UbaTestFile")))
			return logger.Error(TC("Found non-existing file UbaTestFile"));

		if (!FileExists(logger, TC("UbaTestFile2")))
			return logger.Error(TC("Failed to find file UbaTestFile2"));

		StringBuffer<> currentDir;
		if (!GetCurrentDirectoryW(currentDir))
			return logger.Error(TC("GetCurrentDirectoryW failed"));

		bool foundFile = false;
		if (!TraverseDir(logger, currentDir.data, [&](const DirectoryEntry& de) { foundFile |= TStrcmp(de.name, TC("UbaTestFile2")) == 0; }, true))
			return logger.Error(TC("Failed to TraverseDir '.'"));

		if (!foundFile)
			return logger.Error(TC("Did not find UbaTestFile2 with TraverseDir"));

		if (!DeleteFileW(TC("UbaTestFile2")))
			return false;

		LoggerWithWriter nullLogger(g_nullLogWriter);
		if (TraverseDir(nullLogger, TC("TestDir2"), [&](const DirectoryEntry&) {}, true))
			return logger.Error(TC("TraverseDir failed to report fail on non existing dir"));

		return true;
	}

	bool TestMemoryBlock(Logger& logger, const StringBufferBase& rootDir)
	{
		MemoryBlock block(1024 * 1024);
		u64* mem = (u64*)block.Allocate(8, TC("Foo"));
		*mem = 0x1234;
		block.Free(mem);
		return true;
	}
}
