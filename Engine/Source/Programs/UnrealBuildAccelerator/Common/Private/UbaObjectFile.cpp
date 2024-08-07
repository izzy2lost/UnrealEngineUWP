// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFile.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaObjectFileCoff.h"
#include "UbaObjectFileElf.h"
#include "UbaObjectFileLLVMIR.h"

namespace uba
{
	u8 SymbolFileVersion = 1;

	ObjectFile* ObjectFile::OpenAndParse(Logger& logger, const tchar* filename)
	{
		auto file = new FileAccessor(logger, filename);
		auto fileGuard = MakeGuard([&]() { delete file; });

		if (!file->OpenMemoryRead())
			return nullptr;

		ObjectFile* objectFile = Parse(logger, file->GetData(), file->GetSize(), filename);
		if (!objectFile)
			return nullptr;

		fileGuard.Cancel();
		objectFile->m_file = file;
		return objectFile;
	}

	ObjectFile* ObjectFile::Parse(Logger& logger, u8* data, u64 dataSize, const tchar* hint)
	{
		ObjectFile* objectFile = nullptr;

		if (IsElfFile(data, dataSize))
			objectFile = new ObjectFileElf();
		else if (IsLLVMIRFile(data, dataSize))
			objectFile = new ObjectFileLLVMIR();
		else if (IsCoffFile(data, dataSize))
			objectFile = new ObjectFileCoff();
		else
		{
			logger.Error(TC("Unknown object file format. Maybe msvc FE IL? (%s)"), hint);
			return nullptr;
		}

		objectFile->m_data = data;
		objectFile->m_dataSize = dataSize;

		if (objectFile->Parse(logger, hint))
			return objectFile;

		delete objectFile;
		return nullptr;
	}

	bool ObjectFile::CopyMemoryAndClose()
	{
		u8* data = (u8*)malloc(m_dataSize);
		memcpy(data, m_data, m_dataSize);
		m_data = data;
		m_ownsData = true;
		delete m_file;
		m_file = nullptr;
		return true;
	}

	bool ObjectFile::StripExports(Logger& logger)
	{
		u32 keptExportCount = 0;
		return StripExports(logger, m_data, {}, keptExportCount);
		return true;
	}

	bool ObjectFile::WriteImportsAndExports(Logger& logger, MemoryBlock& memoryBlock)
	{
		auto write = [&](const void* data, u64 dataSize) { memcpy(memoryBlock.Allocate(dataSize, 1, TC("")), data, dataSize); };

		write(&SymbolFileVersion, 1);
		write(&m_type, 1);

		// Write all imports
		for (auto& symbol : m_imports)
		{
			write(symbol.c_str(), symbol.size());
			write("", 1);
		}
		write("", 1);

		// Write all exports
		for (auto& kv : m_exports)
		{
			write(kv.first.c_str(), kv.first.size());
			write(kv.second.c_str(), kv.second.size());
			write("", 1);
		}
		write("", 1);
		return true;
	}

	bool ObjectFile::WriteImportsAndExports(Logger& logger, const tchar* exportsFilename)
	{
		FileAccessor exportsFile(logger, exportsFilename);
		if (!exportsFile.CreateWrite())
			return false;

		char buffer[256*1024];
		u64 bufferPos = 0;
		auto flush = [&]() { exportsFile.Write(buffer, bufferPos); bufferPos = 0; };
		auto write = [&](const void* data, u64 dataSize) { if (bufferPos + dataSize > sizeof(buffer)) flush(); memcpy(buffer + bufferPos, data, dataSize); bufferPos += dataSize; };

		// Write all imports
		for (auto& symbol : m_imports)
		{
			write(symbol.c_str(), symbol.size());
			write("", 1);
		}
		write("", 1);

		// Write all exports
		for (auto& kv : m_exports)
		{
			write(kv.first.c_str(), kv.first.size());
			write(kv.second.c_str(), kv.second.size());
			write("", 1);
		}
		write("", 1);

		flush();

		return exportsFile.Close();
	}

	ObjectFile::~ObjectFile()
	{
		if (m_ownsData)
			free(m_data);
		delete m_file;
	}

	const tchar* ObjectFile::GetFileName() const
	{
		return m_file->GetFileName();
	}

	const UnorderedSymbols& ObjectFile::GetImports() const
	{
		return m_imports;
	}

	const UnorderedExports& ObjectFile::GetExports() const
	{
		return m_exports;
	}

	const UnorderedSymbols& ObjectFile::GetPotentialDuplicates() const
	{
		return m_potentialDuplicates;
	}

	bool ObjectFile::CreateExtraFile(Logger& logger, const tchar* extraObjFilename, ObjectFileType type, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile)
	{
		ObjectFileCoff objectFileCoff;
		ObjectFileElf objectFileElf;
		
		ObjectFile& objectFile = type == ObjectFileType_Coff ? (ObjectFile&)objectFileCoff : (ObjectFile&)objectFileElf;


		MemoryBlock memoryBlock(16*1024*1024);

		if (!objectFile.CreateExtraFile(logger, memoryBlock, allNeededImports, allSharedImports, allSharedExports, includeExportsInFile))
			return false;

		FileAccessor extraFile(logger, extraObjFilename);
		if (!extraFile.CreateWrite())
			return false;

		if (!extraFile.Write(memoryBlock.memory, memoryBlock.writtenSize))
			return false;

		return extraFile.Close();
	}

	bool SymbolFile::ParseFile(Logger& logger, const tchar* filename)
	{
		FileAccessor symFile(logger, filename);
		if (!symFile.OpenMemoryRead())
			return false;
		auto readPos = (const char*)symFile.GetData();

		u8 version = *(u8*)readPos++;
		if (SymbolFileVersion != version)
			return logger.Error(TC("%s - Import/export file version mismatch"), filename);

		type = *(const ObjectFileType*)readPos++;

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
}
