// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaDetoursShared.h"
#include "UbaDetoursFileMappingTable.h"
#include "UbaDirectoryTable.h"
#include "UbaTimer.h"

namespace uba
{

	EARLY_INIT StringBuffer<512> g_virtualApplication;
	EARLY_INIT StringBuffer<512> g_virtualApplicationDir;
	EARLY_INIT ProcessStats g_stats;
	EARLY_INIT ReaderWriterLock g_communicationLock;
	EARLY_INIT StringBuffer<256> g_logName;
	EARLY_INIT StringBuffer<512> g_virtualWorkingDir;
	EARLY_INIT StringBuffer<128> g_systemRoot;
	EARLY_INIT StringBuffer<128> g_systemTemp;
	bool g_echoOn = true;

	// This is annoyingly complicated but it comes down to initialization ordering. On windows we detour everything at a certain point in time
	// but on linux everything is detoured from the start. This becomes problematic with these statics and we have to use some party tricks to
	// make sure they are initialized before detoured functions are called.
	// (If this doesn't work we will need to resort to Functions returning a static variable)

	static u8 g_memoryBlockMem[sizeof(MemoryBlock)];
	MemoryBlock& g_memoryBlock = *(MemoryBlock*)g_memoryBlockMem;

	static u8 g_directoryTableMem[sizeof(DirectoryTable)];
	DirectoryTable& g_directoryTable = *(DirectoryTable*)g_directoryTableMem;

	static u8 g_mappedFileTableMem[sizeof(MappedFileTable)];
	MappedFileTable& g_mappedFileTable = *(MappedFileTable*)g_mappedFileTableMem;

	void InitStatics()
	{
		new (g_memoryBlockMem) MemoryBlock(160 * 1024 * 1024); // Leak this
		new (g_directoryTableMem) DirectoryTable(&g_memoryBlock); // Leak this to speed up shutdown
		new (g_mappedFileTableMem) MappedFileTable(g_memoryBlock); // Leak this to speed up shutdown
	}


	#if PLATFORM_WINDOWS
	static bool g_staticInit = []() { InitStatics(); return true; }();
	#else
	static void __attribute__((constructor(102))) CtorInitStatics() { InitStatics(); }
	#endif


	u32 g_rulesIndex;
	ApplicationRules* g_rules;
	bool g_runningRemote;
	bool g_isChild;

#if UBA_DEBUG_LOG_ENABLED
	FileHandle g_debugFile = InvalidFileHandle;
	void WriteDebug(const char* str, u32 strLen);
	thread_local u64 t_logScopeCount;
	constexpr const char g_emptyString[] = "                                                     ";
	constexpr const char* g_emptyStringEnd = ((const char*)g_emptyString) + sizeof_array(g_emptyString) - 1;
	thread_local StringBuffer<LogBufSize> t_a;
	thread_local char t_b[LogBufSize];
	thread_local u32 t_b_size;

	StringBufferBase& GetLogTlsBuffer()
	{
		return t_a;
	}
	void GetPrefixExtra(StringBufferBase& out)
	{
		#if 0
		static u64 startTime = GetTime();
		u64 timeMs = TimeToMs(GetTime() - startTime);
		u64 ms = timeMs % 1000;
		u64 s = timeMs / 1000;

		out.Appendf(TC("[%5llu.%03llu]"), s, ms);
		#endif
		//out.Appendf(TC("[%7u]"), GetCurrentThreadId());
	}
	void WriteDebugLogWithPrefix(const char* prefix, LogScope& scope)
	{
		u32 size__ = t_b_size;
		StringBuffer<128> extra;
		GetPrefixExtra(extra);

		#if PLATFORM_WINDOWS
		u32 res__ = sprintf_s(t_b + size__, LogBufSize - size__, "%s %S   %s%S", prefix, extra.data, g_emptyStringEnd - t_logScopeCount * 2, t_a.data);
		#else
		u32 res__ = snprintf(t_b + size__, LogBufSize - size__, "%s %s   %s%s", prefix, extra.data, g_emptyStringEnd - t_logScopeCount * 2, t_a.data);
		#endif
		if (res__ != -1)
			t_b_size += res__;
		scope.Flush();
	}
	void WriteDebugLog()
	{
		#if PLATFORM_WINDOWS
		t_b_size = sprintf_s(t_b, LogBufSize, "%S", t_a.data);
		WriteDebug(t_b, t_b_size);
		#else
		WriteDebug(t_a.data, t_a.count);
		#endif
	}
	LogScope::LogScope()
	{
		++t_logScopeCount;
	}
	LogScope::~LogScope()
	{
		--t_logScopeCount;
		if (!t_logScopeCount && t_b_size)
			Flush();
	}
	void LogScope::Flush()
	{
		WriteDebug(t_b, t_b_size);
		t_b_size = 0;
		t_b[0] = 0;
	}
#endif

#if UBA_DEBUG_VALIDATE
	bool g_validateFileAccess = false;
#endif

	thread_local u32 t_disallowDetour = 0; // Set this to 1 to disallow all detouring of I/O interaction
	SuppressDetourScope::SuppressDetourScope() { ++t_disallowDetour; }
	SuppressDetourScope::~SuppressDetourScope() { --t_disallowDetour; }


	bool FixPath(StringBufferBase& out, const tchar* path)
	{
		return FixPath2(path, g_virtualWorkingDir.data, g_virtualWorkingDir.count, out.data, &out.count);
	}

	const tchar* GetApplicationShortName()
	{
		if (const tchar* lastBackslash = TStrrchr(g_virtualApplication.data, '\\'))
			return lastBackslash + 1;
		return g_virtualApplication.data;
	}

	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		tchar buffer[1024];
		if (Tvsprintf_s(buffer, sizeof_array(buffer), format, arg) <= 0)
			TStrcpy_s(buffer, sizeof_array(buffer), format);
		va_end(arg);
		StringBuffer<2048> sb;
		sb.Append(GetApplicationShortName()).Append(TC(" ERROR: ")).Append(buffer);
		Rpc_WriteLog(sb.data, sb.count, true);

		#if PLATFORM_WINDOWS // Maybe all platforms should call exit()?
		ExitProcess(code);
		#else
		exit(code);
		#endif
	}

	void Rpc_WriteLog(const tchar* text, u64 textCharLength, bool printInSession)
	{
		#if PLATFORM_WINDOWS
		DEBUG_LOG(TC("LOG  %.*ls"), u32(textCharLength), text); // TODO: Investigate, deadlocks on non-windows
		#endif	
		TimerScope ts(g_stats.log);
		ScopedWriteLock pcs(g_communicationLock);
		BinaryWriter writer;
		writer.WriteByte(MessageType_Log);
		writer.WriteBool(printInSession);
		//writer.WriteBool(hConsoleOutput == g_stderrHandle);
		writer.WriteString(text, textCharLength);
		writer.Flush();
	}

	void Rpc_WriteLogf(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		tchar buffer[1024];
		int count = Tvsprintf_s(buffer, 1024, format, arg);
		if (count <= 0)
		{
			TStrcpy_s(buffer, 1024, format);
			count = int(TStrlen(buffer));
		}
		va_end(arg);
		Rpc_WriteLog(buffer, u32(count), false);
	}

	//TODO: Implement SetConsoleTextAttribute.. clang is using it to color errors

	tchar g_consoleString[4096];
	u32 g_consoleStringIndex;
	EARLY_INIT ReaderWriterLock g_consoleStringCs;

	template<typename CharType>
	void Shared_WriteConsoleT(const CharType* chars, u32 charCount)
	{
		if (!g_echoOn)
			return;

		ScopedWriteLock lock(g_consoleStringCs);
		const CharType* read = chars;
		tchar* write = g_consoleString + g_consoleStringIndex;
		int left = sizeof_array(g_consoleString) - g_consoleStringIndex - 1;
		int available = charCount;
		while (available)
		{
			if (*read == '\n' || !left)
			{
				*write = 0;
				u32 strLen = u32(write - g_consoleString);
				if (!g_rules->SuppressLogLine(g_consoleString, strLen))
					Rpc_WriteLog(g_consoleString, strLen, false);
				write = g_consoleString;
				left = sizeof_array(g_consoleString) - 1;
			}
			else
			{
				*write = *read;
				++write;
			}
			++read;
			--left;
			--available;
		}
		g_consoleStringIndex = u32(write - g_consoleString);
	}

	void Shared_WriteConsole(const char* chars, u32 charCount) { Shared_WriteConsoleT(chars, charCount); }

	#if PLATFORM_WINDOWS
	void Shared_WriteConsole(const wchar_t* chars, u32 charCount) { Shared_WriteConsoleT(chars, charCount); }
	#endif


	const tchar* Shared_GetFileAttributes(FileAttributes& outAttr, const tchar* fileName, bool checkIfDir)
	{
		StringBuffer<> fileNameForKey;
		fileNameForKey.Append(fileName);
		if (CaseInsensitiveFs)
			fileNameForKey.MakeLower();

		UBA_ASSERT(fileNameForKey.count);
		CHECK_PATH(fileNameForKey.data);
		StringKey fileNameKey = ToStringKey(fileNameForKey);

		memset(&outAttr.data, 0, sizeof(outAttr.data));

		bool keepInMemory = KeepInMemory(fileName, fileNameForKey.count);
		if (keepInMemory)
		{
			ScopedReadLock lock(g_mappedFileTable.m_lookupLock);
			auto it = g_mappedFileTable.m_lookup.find(fileNameKey);
			if (it == g_mappedFileTable.m_lookup.end() || it->second.deleted)
			{
				if (StartsWith(fileName, g_systemTemp.data))
				{
					outAttr.useCache = false;
					return fileName;
				}
				outAttr.useCache = true;
				outAttr.exists = false;
				outAttr.lastError = ErrorFileNotFound;
			}
			else
			{
				outAttr.useCache = true;
				outAttr.exists = true;
				outAttr.lastError = ErrorSuccess;
#if PLATFORM_WINDOWS
				LARGE_INTEGER li = ToLargeInteger(it->second.size);
				outAttr.data.nFileSizeLow = li.LowPart;
				outAttr.data.nFileSizeHigh = li.HighPart;
				outAttr.data.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
#else
				UBA_ASSERT(false);
#endif
				// TODO: Currently only used for waccess... need to implement below
				//outAttr.data.ftLastWriteTime = ;
				//outAttr.volumeSerial =
				//outAttr.fileIndex = 
			}
		}
#if PLATFORM_WINDOWS
		else if (fileName[1] == ':' && fileName[3] == 0 && (ToLower(fileName[0]) == ToLower(g_virtualWorkingDir[0]) || ToLower(fileName[0]) == g_systemRoot[0]))
		{
			// This is the root of the drive.. let's just return it as a directory
			outAttr.useCache = true;
			outAttr.exists = true;
			outAttr.lastError = ErrorSuccess;
			outAttr.data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		}
#else
		//else if (StartsWith(fileName, g_applicationDir))
		//{
		//	outAttr.useCache = false;
		//	return fileName;
		//}
#endif
		else if (g_allowDirectoryCache)
		{
			bool isInsideSystemTemp = StartsWith(fileName, g_systemTemp.data);
			// This is an optimization where we populate directory table and use that to figure out if file exists or not..
			// .. in msvc's case it doesn't matter much because these tables are already up to date when msvc use CreateFile.
			// .. clang otoh is using CreateFile with tooons of different paths trying to open files.. in remote worker case this becomes super expensive
			if (!isInsideSystemTemp) // We need to skip SystemTemp.. lots of stuff going on there.
			{
				u32 dirTableOffset = Rpc_GetEntryOffset(fileNameKey, fileName, fileNameForKey.count, checkIfDir);

				if (dirTableOffset == ~u32(0))
				{
					if (g_runningRemote) // This could be a written file not reported to server yet
					{
						ScopedReadLock lock(g_mappedFileTable.m_lookupLock);
						auto findIt = g_mappedFileTable.m_lookup.find(fileNameKey);
						if (findIt != g_mappedFileTable.m_lookup.end() && !findIt->second.deleted)
						{
							outAttr.useCache = false;
							return findIt->second.name;
						}
					}

					outAttr.useCache = true;
					outAttr.exists = false;
					outAttr.lastError = ErrorFileNotFound;
				}
				else
				{
					DirectoryTable::EntryInformation info;
					g_directoryTable.GetEntryInformation(info, dirTableOffset);

					if (info.attributes)
					{
						outAttr.useCache = true;
						outAttr.exists = true;
						outAttr.lastError = ErrorSuccess;

						UBA_ASSERT(info.fileIndex);
						outAttr.fileIndex = info.fileIndex;
						outAttr.volumeSerial = info.volumeSerial;

#if PLATFORM_WINDOWS
						LARGE_INTEGER li = ToLargeInteger(info.size);
						outAttr.data.dwFileAttributes = info.attributes;
						outAttr.data.nFileSizeLow = li.LowPart;
						outAttr.data.nFileSizeHigh = li.HighPart;
						(u64&)outAttr.data.ftCreationTime = info.lastWrite;
						(u64&)outAttr.data.ftLastAccessTime = info.lastWrite;
						(u64&)outAttr.data.ftLastWriteTime = info.lastWrite;
#else
						outAttr.data.st_mtime = info.lastWrite;
						outAttr.data.st_mode = (mode_t)info.attributes;
						outAttr.data.st_dev = info.volumeSerial;
						outAttr.data.st_ino = info.fileIndex;
						outAttr.data.st_size = info.size;
#endif
					}
					else
					{
						// File used to exist but was deleted
						outAttr.useCache = true;
						outAttr.exists = false;
						outAttr.lastError = ErrorFileNotFound;
					}
				}
			}
			else
			{
				outAttr.useCache = false;
				return fileName;
			}
		}
		else
		{
			outAttr.useCache = false;
			return fileName;
		}

#if 0//UBA_DEBUG_VALIDATE
		if (g_validateFileAccess && !keepInMemory)
		{
			WIN32_FILE_ATTRIBUTE_DATA validate;
			memset(&validate, 0, sizeof(validate));
			SuppressDetourScope _;
			BOOL res = True_GetFileAttributesExW(fileName, GetFileExInfoStandard, &validate); (void)res;
			if (outAttr.exists)
			{
				UBA_ASSERTF(res != 0, L"File %ls exists even though uba claims it is not..", fileName);
				if (validate.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					UBA_ASSERTF((outAttr.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), L"File attributes are wrong for %ls", fileName);
				else
				{
					validate.ftCreationTime = outAttr.data.ftCreationTime; // Creation time is not really important
					validate.ftLastAccessTime = outAttr.data.ftLastAccessTime; // Access time is not really important
					validate.ftLastWriteTime = outAttr.data.ftLastWriteTime; // Write time is important, revisit this
					UBA_ASSERTF(memcmp(&validate, &outAttr.data, sizeof(WIN32_FILE_ATTRIBUTE_DATA)) == 0, L"File %ls is not up-to-date in cache", fileName);
				}
			}
			else
			{
				UBA_ASSERTF(res == 0, L"Can't find file %ls but validation checked that it is there", fileName); // This means most likely that Uba did not update attribute table for added files.
				DWORD lastError2 = GetLastError();
				if (lastError2 == ERROR_PATH_NOT_FOUND || lastError2 == ERROR_INVALID_NAME)
					lastError2 = ERROR_FILE_NOT_FOUND;
				UBA_ASSERT(outAttr.lastError == lastError2);
			}
		}
#endif
		return fileName;
	}
}
