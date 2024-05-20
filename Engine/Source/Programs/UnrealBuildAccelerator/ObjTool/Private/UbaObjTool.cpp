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
		logger.Info(TC(""));
		return -1;
	}

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

		struct SymbolFile
		{
			UnorderedSymbols imports;
			UnorderedExports exports;

			bool ParseFile(Logger& logger, const tchar* filename)
			{
				FileAccessor symFile(logger, filename);
				if (!symFile.OpenMemoryRead())
					return false;
				auto readPos = (const char*)symFile.GetData();

				while (*readPos)
				{
					auto strEnd = strlen(readPos);
					imports.insert(std::string(readPos, readPos + strEnd));
					readPos = readPos + strEnd + 1;
				}
				++readPos;

				while (*readPos)
				{
					auto strEnd = strlen(readPos);
					std::string extra;
					if (const char* comma = strchr(readPos, ','))
					{
						strEnd = comma - readPos;
						extra = comma;
					}
					exports.emplace(std::string(readPos, readPos + strEnd), extra);
					readPos = readPos + strEnd + 1;
				}
				return true;
			}
		};

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
					const TString& objFileName = *it;

					if (EndsWith(objFileName.c_str(), objFileName.size(), TC(".sym")))
					{
						SymbolFile symbolFile;
						if (!symbolFile.ParseFile(logger, objFileName.c_str()))
						{
							success = false;
							return;
						}
						ScopedCriticalSection _(cs);
						allNeededImports.insert(symbolFile.imports.begin(), symbolFile.imports.end());
					}
					else
					{
						ObjectFile* objectFile = ObjectFile::OpenAndParse(logger, objFileName.c_str());
						if (!objectFile)
						{
							success = false;
							return;
						}
						auto g = MakeGuard([&]() { delete objectFile; });

						ScopedCriticalSection _(cs);
						allNeededImports.insert(objectFile->GetImports().begin(), objectFile->GetImports().end());
					}
				});
			if (!success)
				return -1;

			UnorderedSymbols allSharedImports; // Imports from all the obj files about to be stripped
			UnorderedExports allSharedExports; // Exports from all the obj files about to be stripped

			Map<TString, ObjectFile*> objectFiles;
			auto g = MakeGuard([&]() { for (auto& kv : objectFiles) delete kv.second; });

			workManager.ParallelFor(workerCount, objFilesToStrip, [&](auto& it)
				{
					const TString& objFileName = *it;

					if (EndsWith(objFileName.c_str(), objFileName.size(), TC(".sym")))
					{
						SymbolFile symbolFile;
						if (!symbolFile.ParseFile(logger, objFileName.c_str()))
						{
							success = false;
							return;
						}
						ScopedCriticalSection _(cs);
						allSharedImports.insert(symbolFile.imports.begin(), symbolFile.imports.end());
						allSharedExports.insert(symbolFile.exports.begin(), symbolFile.exports.end());
					}
					else
					{
						ObjectFile* objectFile = ObjectFile::OpenAndParse(logger, objFileName.c_str());
						if (!objectFile)
						{
							success = false;
							return;
						}

						ScopedCriticalSection _(cs);
						objectFiles.try_emplace(objFileName, objectFile);
						allSharedExports.insert(objectFile->GetExports().begin(), objectFile->GetExports().end());
					}
				});
			if (!success)
				return -1;

			// Figure out which loopback symbols that should be added to which obj file
			if (!objectFiles.empty())
			{
				UnorderedSymbols duplicates;
				for (auto& objFileName : objFilesToStrip)
					if (!objectFiles[objFileName]->ComputeLoopbacksAndDuplicates(allSharedExports, duplicates))
						return -1;

				Atomic<u32> totalExportCount;
				Atomic<u32> totalKeptExportCount;
				workManager.ParallelFor(workerCount, objectFiles, [&](auto& it)
					{
						ObjectFile& file = *it->second;
						const tchar* fileName = file.GetFileName();
						const tchar* lastDot = TStrrchr(fileName, '.');
						UBA_ASSERT(lastDot);
						StringBuffer<> newFilename;
						newFilename.Append(fileName, lastDot - fileName).Append(TC(".strip")).Append(lastDot);
						u32 keptExportCount = 0;
						if (!file.CreateStripped(logger, newFilename.data, allNeededImports, keptExportCount))
							success = false;

						totalExportCount += u32(file.GetExports().size());
						totalKeptExportCount += keptExportCount;
					});
				if (!success)
					return -1;
			}
			else if (!extraObjFile.empty())
			{
				if (!ObjectFile::CreateExtraFile(logger, extraObjFile.c_str(), allNeededImports, allSharedImports, allSharedExports))
					return -1;
			}

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
				exportsFile.Append(fileName, lastDot - fileName).Append(TC(".sym"));

				if (!objectFile->WriteSymbols(logger, exportsFile.data))
					return false;

				u32 keptExportCount = 0;

				StringBuffer<> newFilename;
				newFilename.Append(fileName, lastDot - fileName).Append(TC(".TEST")).Append(lastDot);
				if (!objectFile->CreateStripped(logger, newFilename.data, {}, keptExportCount))
					return false;
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
