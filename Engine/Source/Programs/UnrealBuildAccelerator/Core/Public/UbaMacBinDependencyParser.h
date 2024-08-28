// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStringBuffer.h"

#if !defined(UBA_IS_DETOURED_INCLUDE)
#define TRUE_WRAPPER(func) func
#endif

namespace uba
{
	inline bool FindImportsMac(const tchar* fileName, const Function<void(const tchar* import, bool isKnown)>& func)
	{
#if 0
		HANDLE fileHandle = ::CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return true;
		auto closeFileHandle = MakeGuard([&]() { CloseHandle(fileHandle); });
		HANDLE fileMapping = ::CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
		if (!fileMapping)
			return false;
		auto closeMappingHandle = MakeGuard([&]() { ::CloseHandle(fileMapping); });
		void* mem = ::MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
		if (!mem)
			return false;
		auto unmap = MakeGuard([&]() { ::UnmapViewOfFile(mem); });

		auto it = (const u8*)mem;
		while (true)
		{
			if (*it++ != '@')
				continue;

			if (memcmp(it, "executable_path", 14) == 0)
				break;

			if (memcmp(it, "rpath/", 6) != 0)
				continue;
			it += 6;
			auto importFile = (const char*)it;
			if (!strstr(importFile, ".dylib"))
				return false;

#if PLATFORM_WINDOWS
			StringBuffer<> temp;
			temp.Append(importFile);
			func(temp.data, false);
#else
			func(importFile, false);
#endif
		}
#endif
		return true;
	}
}