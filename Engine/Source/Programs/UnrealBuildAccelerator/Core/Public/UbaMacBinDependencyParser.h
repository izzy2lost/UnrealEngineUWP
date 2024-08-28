// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStringBuffer.h"

#if !defined(UBA_IS_DETOURED_INCLUDE)
#define TRUE_WRAPPER(func) func
#endif

namespace uba
{
	inline bool FindImportsMac(const tchar* fileName, const Function<void(const tchar* import, bool isKnown)>& func, StringBufferBase& outError)
	{
		int fd = open(fileName, O_RDONLY);
		if (fd == -1)
		{
			outError.Appendf("Open failed for file %s", fileName);
			return false;
		}
		auto closeFileHandle = MakeGuard([&]() { close(fd); });
		struct stat sb;
		if (fstat(fd, &sb) == -1)
		{
			outError.Appendf("Stat failed for file %s", fileName);
			return false;
		}
		u32 size = Min(u32(sb.st_size), 8048u);

		void* mem = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (mem == MAP_FAILED)
		{
			outError.Appendf("Mmap failed for file %s", fileName);
			return false;
		}
		auto unmap = MakeGuard([&]() { munmap(mem, size); });

		auto it = (const u8*)mem;
		auto end = it + size - 14;
		while (it != end)
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
			if (mem == MAP_FAILED)
			{
				outError.Appendf("Found @rpath in binary %s that did not end with .dylib (%s)", fileName, importFile);
				return false;
			}

			func(importFile, false);
		}
		return true;
	}
}