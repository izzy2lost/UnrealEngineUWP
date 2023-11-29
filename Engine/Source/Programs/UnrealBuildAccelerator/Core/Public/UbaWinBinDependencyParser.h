// Copyright Epic Games, Inc. All Rights Reserved.

namespace uba
{

	DWORD GetOffset(DWORD rva, PIMAGE_SECTION_HEADER psh, PIMAGE_NT_HEADERS pnt)
	{
		if(rva == 0)
			return rva;
		PIMAGE_SECTION_HEADER seh = psh;
		for(size_t i=0; i<pnt->FileHeader.NumberOfSections; ++i, ++seh)
			if (rva >= seh->VirtualAddress && rva < seh->VirtualAddress + seh->Misc.VirtualSize)
				break;
		return rva - seh->VirtualAddress + seh->PointerToRawData;
	} 

	bool FindImportsInMem(LPCWSTR fileName, void* mem, const Function<void(const char* import)>& func)
	{
		auto hdrs = (PIMAGE_NT_HEADERS)(PCHAR(mem) + PIMAGE_DOS_HEADER(mem)->e_lfanew);   
		auto pSech=IMAGE_FIRST_SECTION(hdrs);
		__try
		{
			auto& dataDir = hdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
			if(dataDir.Size == 0)
				return true; // No import table

			auto importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)mem + GetOffset(dataDir.VirtualAddress, pSech, hdrs));
			while(importDesc->Name != NULL)
			{
				const char* name = (PCHAR)((DWORD_PTR)mem + GetOffset(importDesc->Name, pSech, hdrs));
				func(name);
				++importDesc;

			}
		}
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			StringBuffer<> buf;
			buf.Appendf(L"Access violation reading %s", fileName);
			UbaAssert(buf.data, __FILE__, __LINE__, "", GetExceptionCode());
			return false;
		}
		return true;
	}

	bool FindImports(LPCWSTR fileName, const Function<void(const char* import)>& func)
	{
		HANDLE fileHandle = True_CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return true;
		auto closeFileHandle = MakeGuard([&]() { CloseHandle(fileHandle); });
		HANDLE fileMapping = True_CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
		if (!fileMapping)
			return false;
		auto closeMappingHandle = MakeGuard([&]() { CloseHandle(fileMapping); });
		void* mem = True_MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
		if (!mem)
			return false;
		auto unmap = MakeGuard([&]() { True_UnmapViewOfFile(mem); });

		return FindImportsInMem(fileName, mem, func);
	}
}
