// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaLogger.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"

#if PLATFORM_WINDOWS
#include <io.h>
#define Fputs fputws
#else
#define Fputs fputs
#define _vsnwprintf_s(buffer,capacity,count,format,args) vsnprintf(buffer, capacity, format, args) // TODO: This will overflow
#endif


namespace uba
{
	ANALYSIS_NORETURN void UbaAssert(const tchar* text, const char* file, u32 line, const char* expr, u32 terminateCode)
	{
		static ReaderWriterLock assertLock;
		ScopedWriteLock lock(assertLock);

		StringBuffer<4096> b;
		WriteAssertInfo(b, text, file, line, expr, 1);
		Fputs(b.data, stdout);
		Fputs(TC("\n"), stdout);
		fflush(stdout);

#if PLATFORM_WINDOWS
#if UBA_ASSERT_MESSAGEBOX
		int ret = MessageBoxW(GetConsoleWindow(), b.data, TC("Assert"), MB_ABORTRETRYIGNORE);
		if (ret != IDABORT)
		{
			if (ret == IDRETRY)
				DebugBreak();
			return;
		}

		SetFocus(GetConsoleWindow());
		SetActiveWindow(GetConsoleWindow());
#endif
		ExitProcess(terminateCode);
#else
		exit(-1);
#endif
	}

	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		tchar buffer[1024];
		int count = TSprintf_s(buffer, 1024, format, arg);
		if (count <= 0)
			TStrcpy_s(buffer, 1024, format);
		va_end(arg);
#if PLATFORM_WINDOWS
		wprintf(TC("FATAL ERROR %u: %s\n"), code, buffer);
		fflush(stdout);
		ExitProcess(code);
#else
		printf(TC("FATAL ERROR %u: %s\n"), code, buffer);
		fflush(stdout);
		exit(int(code));
#endif
	}

	void Logger::Info(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Info, format, arg);
		va_end(arg);
	}

	void Logger::Detail(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Detail, format, arg);
		va_end(arg);
	}

	void Logger::Debug(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Debug, format, arg);
		va_end(arg);
	}

	void Logger::Warning(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Warning, format, arg);
		va_end(arg);
	}

	bool Logger::Error(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Error, format, arg);
		va_end(arg);
		return false;
	}

	void Logger::Logf(LogEntryType type, const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(type, format, arg);
		va_end(arg);
	}

	void Logger::LogArg(LogEntryType type, const tchar* format, va_list& args)
	{
		#if !PLATFORM_WINDOWS
		constexpr size_t _TRUNCATE = (size_t)-1;
		#endif

		tchar buffer[1024];
		int len = _vsnwprintf_s(buffer, sizeof_array(buffer), _TRUNCATE, format, args);
		if (len != -1)
		{
			Log(type, buffer, u32(len));
		}
		else
		{
			Vector<tchar> buf;
			buf.resize(64 * 1024);
			_vsnwprintf_s(buf.data(), buf.size(), buf.size(), format, args);
			Log(type, buf.data(), u32(buf.size()));
		}
	}

	LoggerWithWriter::LoggerWithWriter(LogWriter& writer, const tchar* prefix)
	:	m_writer(writer)
	,	m_prefix(prefix)
	,	m_prefixLen(prefix ? u32(TStrlen(prefix)) : 0)
	{
	}

	void FilteredLogWriter::Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
	{
		if (type > m_level)
			return;
		m_writer.Log(type, str, strLen, prefix, prefixLen);
	}

	class ConsoleLogWriter : public LogWriter
	{
	public:
		ConsoleLogWriter();
		virtual void BeginScope() override;
		virtual void EndScope() override;
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override;
	private:
		void LogNoLock(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen);
		ReaderWriterLock m_lock;
#if PLATFORM_WINDOWS
		HANDLE m_stdout = 0;
		u32 m_defaultAttributes = 0;
#endif
		u32 m_scopeCount = 0;
	} g_consoleLogWriterImpl;
	LogWriter& g_consoleLogWriter = g_consoleLogWriterImpl;

	class NullLogWriter : public LogWriter
	{
	public:
		virtual void BeginScope() override {}
		virtual void EndScope() override {}
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override {}
	} g_nullLogWriterImpl;
	LogWriter& g_nullLogWriter = g_nullLogWriterImpl;


	ConsoleLogWriter::ConsoleLogWriter()
	{
#if PLATFORM_WINDOWS
		if (_isatty(_fileno(stdout)))
		{
			m_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(m_stdout, &csbi);
			m_defaultAttributes = csbi.wAttributes;
		}
#endif
	}

	void ConsoleLogWriter::BeginScope()
	{
		if (!m_scopeCount++)
			m_lock.EnterWrite();
	}

	void ConsoleLogWriter::EndScope()
	{
		if (--m_scopeCount)
			return;
#if PLATFORM_WINDOWS
		if (!m_stdout)
#endif
			fflush(stdout);
		m_lock.LeaveWrite();
	}

	void ConsoleLogWriter::Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
	{
		if (m_scopeCount)
			return LogNoLock(type, str, strLen, prefix, prefixLen);
		ScopedWriteLock lock(m_lock);
		LogNoLock(type, str, strLen, prefix, prefixLen);
#if PLATFORM_WINDOWS
		if (!m_stdout)
#endif
			fflush(stdout);
	}

	void ConsoleLogWriter::LogNoLock(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
	{
#if PLATFORM_WINDOWS
		if (!m_stdout)
		{
			if (prefixLen)
			{
				Fputs(prefix, stdout);
				Fputs(TC(" - "), stdout);
			}
			_putws(str);
		}
		else
		{
			if (prefixLen)
			{
				WriteConsoleW(m_stdout, prefix, prefixLen, NULL, NULL);
				WriteConsoleW(m_stdout, TC(" - "), 3, NULL, NULL);
			}
			switch (type)
			{
			case LogEntryType_Warning:
				SetConsoleTextAttribute(m_stdout, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
				WriteConsoleW(m_stdout, str, strLen, NULL, NULL);
				SetConsoleTextAttribute(m_stdout, (WORD)m_defaultAttributes);
				break;
			case LogEntryType_Error:
				SetConsoleTextAttribute(m_stdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
				WriteConsoleW(m_stdout, str, strLen, NULL, NULL);
				SetConsoleTextAttribute(m_stdout, (WORD)m_defaultAttributes);
				break;
			default:
				WriteConsoleW(m_stdout, str, strLen, NULL, NULL);
				break;
			}
			WriteConsoleW(m_stdout, TC("\r\n"), 2, NULL, NULL);
		}
#else
		if (prefixLen)
		{
			Fputs(prefix, stdout);
			Fputs(TC(" - "), stdout);
		}
		Fputs(str, stdout);
		Fputs(TC("\n"), stdout);
#endif
	}

	LastErrorToText::LastErrorToText() : LastErrorToText(GetLastError())
	{
	}

	LastErrorToText::LastErrorToText(u32 lastError)
	{
#if PLATFORM_WINDOWS
		size_t size = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), data, capacity, NULL);
		if (!size)
			AppendValue(lastError);
		else
			Resize(size - 2);
#else
		Append(strerror(int(lastError)));
#endif
	}

	BytesToText::BytesToText(u64 bytes)
	{
		if (bytes < 1000 * 1000)
			TSprintf_s(str, 32, TC("%.1fkb"), double(bytes) / 1000ull);
		else if (bytes < 1000ull * 1000 * 1000)
			TSprintf_s(str, 32, TC("%.1fmb"), double(bytes) / (1000ull * 1000));
		else
			TSprintf_s(str, 32, TC("%.1fgb"), double(bytes) / (1000ull * 1000 * 1000));
	}
}
