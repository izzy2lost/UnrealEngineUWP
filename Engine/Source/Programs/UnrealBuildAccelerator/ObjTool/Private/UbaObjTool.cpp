// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFile.h"
#include "UbaDirectoryIterator.h"
#include "UbaFileAccessor.h"
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
		logger.Info(TC("   Options:"));
		logger.Info(TC("    -printsymbols            Print the symbols found in obj file"));
		logger.Info(TC("    -stripexports            Will strip exports and write them out in a .exp file"));
		logger.Info(TC(""));
		logger.Info(TC("  --- OR ---"));
		logger.Info(TC(""));
		logger.Info(TC("  UbaObjTool.exe @<rspfile>"));
		logger.Info(TC(""));
		logger.Info(TC("   Response file options:"));
		logger.Info(TC("    /S:<objfile>             Obj file to strip. Will produce a .strip.obj file. Multiple allowed"));
		logger.Info(TC("    /D:<objfile>             Obj file depending on obj files to strip. Multiple allowed"));
		logger.Info(TC("    /O:<objfile>             Obj file to output containing exports and loopbacks"));
		logger.Info(TC("    /COMPRESS                Write '/O' file compressed"));
		logger.Info(TC(""));
		return -1;
	}

	// TODO: Add to rsp file instead
	const char* NeededImports[] = 
	{
		"NvOptimusEnablement",
		"AmdPowerXpressRequestHighPerformance",
		"D3D12SDKVersion",
		"D3D12SDKPath",
	};

	int WrappedMain(int argc, tchar* argv[])
	{
		using namespace uba;

		TString objFile;
		bool printSymbols = false;
		bool stripExports = false;

		Vector<TString> objFilesToStrip;
		Vector<TString> objFilesDependencies;
		TString extraObjFile;

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
				else if (name.StartsWith(TC("/O:")))
				{
					extraObjFile = name.data + 3;
				}
				else if (name.Equals(TC("-printsymbols")))
				{
					printSymbols = true;
				}
				else if (name.Equals(TC("-stripexports")))
				{
					stripExports = true;
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
			ObjectFileType type = ObjectFileType_Unknown;

			CriticalSection cs;
			Atomic<bool> success = true;
			UnorderedSymbols allNeededImports; // Imports needed from the outside of the stripped obj files

			for (auto imp : NeededImports)
				allNeededImports.insert(imp);

			u32 workerCount = DefaultProcessorCount;
			WorkManagerImpl workManager(workerCount);
			workManager.ParallelFor(workerCount, objFilesDependencies, [&](auto& it)
				{
					const TString& exiFilename = *it;

					SymbolFile symbolFile;
					if (!symbolFile.ParseFile(logger, exiFilename.c_str()))
					{
						success = false;
						return;
					}
					ScopedCriticalSection _(cs);
					UBA_ASSERT(type == ObjectFileType_Unknown || type == symbolFile.type);
					if (type == ObjectFileType_Unknown)
						type = symbolFile.type;
					allNeededImports.insert(symbolFile.imports.begin(), symbolFile.imports.end());
				});
			if (!success)
				return -1;

			UnorderedSymbols allSharedImports; // Imports from all the obj files about to be stripped
			UnorderedExports allSharedExports; // Exports from all the obj files about to be stripped

			Map<TString, ObjectFile*> objectFiles;
			auto g = MakeGuard([&]() { for (auto& kv : objectFiles) delete kv.second; });

			workManager.ParallelFor(workerCount, objFilesToStrip, [&](auto& it)
				{
					const TString& exiFilename = *it;
					SymbolFile symbolFile;
					if (!symbolFile.ParseFile(logger, exiFilename.c_str()))
					{
						success = false;
						return;
					}
					ScopedCriticalSection _(cs);
					UBA_ASSERT(type == ObjectFileType_Unknown || type == symbolFile.type);
					if (type == ObjectFileType_Unknown)
						type = symbolFile.type;
					allSharedImports.insert(symbolFile.imports.begin(), symbolFile.imports.end());
					allSharedExports.insert(symbolFile.exports.begin(), symbolFile.exports.end());
				});
			if (!success)
				return -1;

			if (!extraObjFile.empty())
				if (!ObjectFile::CreateExtraFile(logger, extraObjFile.c_str(), type, allNeededImports, allSharedImports, allSharedExports, true))
					return -1;

			//logger.Info(TC("Reduced export count from %llu to %llu"), totalExportCount.load(), totalKeptExportCount.size());
		}
		else
		{
			if (objFile.empty())
				return PrintHelp(TC("No obj or rsp file provided"));

			ObjectFile* objectFile = ObjectFile::OpenAndParse(logger, objFile.c_str());
			if (!objectFile)
				return -1;
			auto g = MakeGuard([&](){ delete objectFile; });

			if (printSymbols)
			{
				for (auto& symbol : objectFile->GetImports())
					logger.Info(TC("I %S"), symbol.c_str());

				for (auto& kv : objectFile->GetExports())
					logger.Info(TC("E %S%S"), kv.first.c_str(), kv.second.c_str());
			}

			if (stripExports)
			{
				if (!objectFile->CopyMemoryAndClose())
					return false;

				const tchar* fileName = objFile.c_str();
				const tchar* lastDot = TStrrchr(fileName, '.');
				UBA_ASSERT(lastDot);
				StringBuffer<> exportsFile;
				exportsFile.Append(fileName, lastDot - fileName).Append(TC(".exi"));

				if (!objectFile->WriteImportsAndExports(logger, exportsFile.data))
					return false;

				//u32 keptExportCount = 0;
				//StringBuffer<> newFilename;
				//newFilename.Append(fileName, lastDot - fileName).Append(TC(".TEST")).Append(lastDot);
				//if (!objectFile->CreateStripped(logger, newFilename.data, {}, keptExportCount))
				//	return false;
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
