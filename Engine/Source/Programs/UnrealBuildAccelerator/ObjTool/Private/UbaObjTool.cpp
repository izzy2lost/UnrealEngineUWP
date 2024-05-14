// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFile.h"
#include "UbaDirectoryIterator.h"
#include "UbaVersion.h"
#include "UbaWorkManager.h"

namespace uba
{
	const tchar* Version = GetVersionString();
	u32	DefaultProcessorCount = []() { return GetLogicalProcessorCount(); }();

	int PrintHelp(const tchar* message)
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));
		if (*message)
		{
			logger.Info(TC(""));
			logger.Error(TC("%s"), message);
		}
		const tchar* dbgStr = TC("");
		#if UBA_DEBUG
		dbgStr = TC(" (DEBUG)");
		#endif

		logger.Info(TC(""));
		logger.Info(TC("-------------------------------------------"));
		logger.Info(TC("   UbaObjTool v%s%s"), Version, dbgStr);
		logger.Info(TC("-------------------------------------------"));
		logger.Info(TC(""));
		logger.Info(TC("  UbaObjTool.exe [options...] <objfile>"));
		logger.Info(TC(""));
		logger.Info(TC(""));
		logger.Info(TC("  -printsymbols                    Print the symbols found in obj file."));
		logger.Info(TC(""));
		return -1;
	}

	int WrappedMain(int argc, tchar* argv[])
	{
		using namespace uba;

		TString objFile;
		bool printSymbols = false;

		Vector<TString> objFilesToStrip;
		Vector<TString> objFilesDependencies;

		auto parseArg = [&](const tchar* arg)
			{
				StringBuffer<> name;
				StringBuffer<> value;

				if (const tchar* equals = TStrchr(arg,'='))
				{
					name.Append(arg, equals - arg);
					value.Append(equals+1);
				}
				else
				{
					name.Append(arg);
				}

				if (name.StartsWith(TC("/D:")))
				{
					objFilesDependencies.push_back(name.data + 3);
				}
				else if (name.StartsWith(TC("/S:")))
				{
					objFilesToStrip.push_back(name.data + 3);
				}
				else if (name.Equals(TC("-printsymbols")))
				{
					printSymbols = true;
				}
				else if (name.Equals(TC("-?")))
				{
					return PrintHelp(TC(""));
				}
				else if (objFile.empty() && name[0] != '-' && name[0] != '/')
				{
					objFile = name.data;
					return 0;
				}
				else
				{
					StringBuffer<> msg;
					msg.Appendf(TC("Unknown argument '%s'"), name.data);
					return PrintHelp(msg.data);
				}
				return 0;
			};

		for (int i=1; i!=argc; ++i)
		{
			const tchar* arg = argv[i];
			if (*arg == '@')
			{
				++arg;
				StringBuffer<> temp;
				if (*arg == '\"')
				{
					temp.Append(arg + 1);
					temp.Resize(temp.count - 1);
					arg = temp.data;
				}
				int res = 0;
				LoggerWithWriter logger(g_consoleLogWriter, TC(""));
				if (!ReadLines(logger, arg, [&](const TString& line)
					{
						res = parseArg(line.c_str());
						return res == 0;
					}))
					return -1;
				if (res != 0)
					return res;
				continue;
			}
			int res = parseArg(arg);
			if (res != 0)
				return res;
		}

		FilteredLogWriter logWriter(g_consoleLogWriter, LogEntryType_Info);
		LoggerWithWriter logger(logWriter, TC(""));

		if (!objFilesToStrip.empty())
		{
			CriticalSection cs;
			bool success = true;
			UnorderedSymbols allNeededImports;

			u32 workerCount = 1;//DefaultProcessorCount;
			WorkManagerImpl workManager(workerCount);
			workManager.ParallelFor(workerCount, objFilesDependencies, [&](auto& it)
				{
					const TString& objFile = *it;
					ObjectFile objectFile;
					bool res = objectFile.Parse(logger, objFile.c_str());

					ScopedCriticalSection _(cs);
					success &= res;
					if (res)
						allNeededImports.insert(objectFile.GetImports().begin(), objectFile.GetImports().end());
				});

			if (!success)
				return -1;

			UnorderedSymbols allSharedExports;
			List<ObjectFile> objectFiles;

			workManager.ParallelFor(workerCount, objFilesToStrip, [&](auto& it)
				{
					cs.Enter();
					ObjectFile& objectFile = objectFiles.emplace_back();
					cs.Leave();

					const TString& objFile = *it;
					bool res = objectFile.Parse(logger, objFile.c_str());

					ScopedCriticalSection _(cs);
					success &= res;
					if (res)
						allSharedExports.insert(objectFile.GetExports().begin(), objectFile.GetExports().end());
				});

			if (!success)
				return -1;

			workManager.ParallelFor(workerCount, objectFiles, [&](auto& it)
				{
					auto& objectFile = *it;
					const tchar* fileName = objectFile.GetFileName();
					const tchar* lastDot = TStrrchr(fileName, '.');
					UBA_ASSERT(lastDot);
					StringBuffer<> newFilename;
					newFilename.Append(fileName, lastDot - fileName).Append(TC(".strip")).Append(lastDot);
					objectFile.CreateStripped(logger, newFilename.data, allNeededImports, allSharedExports);
				});

			//logger.Info(TC("Stripped %llu symbols from %llu obj files"), strippedSymbolCount.load(), objFilesToStrip.size());
			if (!success)
				return -1;
		}
		else
		{
			if (objFile.empty())
				return PrintHelp(TC("No obj file provided"));

			ObjectFile objectFile;
			if (!objectFile.Parse(logger, objFile.c_str()))
				return -1;

			if (printSymbols)
			{
				for (auto& symbol : objectFile.GetImports())
					logger.Info(TC("I %S"), symbol.c_str());

				for (auto& symbol : objectFile.GetExports())
					logger.Info(TC("E %S"), symbol.c_str());
			}
		}
		return 0;
	}
}

#if PLATFORM_WINDOWS
int wmain(int argc, wchar_t* argv[])
{
	return uba::WrappedMain(argc, argv);
}
#else
int main(int argc, char* argv[])
{
	return uba::WrappedMain(argc, argv);
}
#endif
