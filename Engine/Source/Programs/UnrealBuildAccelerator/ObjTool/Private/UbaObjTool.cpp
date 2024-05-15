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
			Atomic<bool> success = true;
			UnorderedSymbols allNeededImports; // Imports needed from the outside of the stripped obj files

			u32 workerCount = DefaultProcessorCount;
			WorkManagerImpl workManager(workerCount);
			workManager.ParallelFor(workerCount, objFilesDependencies, [&](auto& it)
				{
					const TString& objFile = *it;
					ObjectFile* objectFile = ObjectFile::CreateAndParse(logger, objFile.c_str());
					if (!objectFile)
					{
						success = false;
						return;
					}
					auto g = MakeGuard([&]() { delete objectFile; });

					ScopedCriticalSection _(cs);
					allNeededImports.insert(objectFile->GetImports().begin(), objectFile->GetImports().end());
				});
			if (!success)
				return -1;

			UnorderedSymbols allSharedExports; // Exports from all the obj files about to be stripped

			Map<TString, ObjectFile*> objectFiles;
			auto g = MakeGuard([&]() { for (auto& kv : objectFiles) delete kv.second; });

			workManager.ParallelFor(workerCount, objFilesToStrip, [&](auto& it)
				{
					const TString& objFileName = *it;
					ObjectFile* objectFile = ObjectFile::CreateAndParse(logger, objFileName.c_str());
					if (!objectFile)
					{
						success = false;
						return;
					}

					ScopedCriticalSection _(cs);
					objectFiles.try_emplace(objFileName, objectFile);
					allSharedExports.insert(objectFile->GetExports().begin(), objectFile->GetExports().end());
				});
			if (!success)
				return -1;

			// Figure out which loopback symbols that should be added to which obj file
			UnorderedSymbols duplicates;
			for (auto& objFileName : objFilesToStrip)
				if (!objectFiles[objFileName]->ComputeLoopbacksAndDuplicates(allSharedExports, duplicates))
					return -1;

			workManager.ParallelFor(workerCount, objectFiles, [&](auto& it)
				{
					ObjectFile& file = *it->second;
					const tchar* fileName = file.GetFileName();
					const tchar* lastDot = TStrrchr(fileName, '.');
					UBA_ASSERT(lastDot);
					StringBuffer<> newFilename;
					newFilename.Append(fileName, lastDot - fileName).Append(TC(".strip")).Append(lastDot);
					if (!file.CreateStripped(logger, newFilename.data, allNeededImports))
						success = false;
				});
			if (!success)
				return -1;

			//logger.Info(TC("Stripped %llu symbols from %llu obj files"), strippedSymbolCount.load(), objFilesToStrip.size());
		}
		else
		{
			if (objFile.empty())
				return PrintHelp(TC("No obj file provided"));

			ObjectFile* objectFile = ObjectFile::CreateAndParse(logger, objFile.c_str());
			if (!objectFile)
				return -1;

			if (printSymbols)
			{
				for (auto& symbol : objectFile->GetImports())
					logger.Info(TC("I %S"), symbol.c_str());

				for (auto& symbol : objectFile->GetExports())
					logger.Info(TC("E %S"), symbol.c_str());
			}

			delete objectFile;
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
