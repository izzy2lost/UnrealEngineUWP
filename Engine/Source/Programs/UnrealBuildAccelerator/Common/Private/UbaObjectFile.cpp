// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFile.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaObjectFileCoff.h"
#include "UbaObjectFileElf.h"

namespace uba
{
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
		if (dataSize < 4)
			return nullptr;

		ObjectFile* objectFile = nullptr;

		constexpr u8 elfMagic[] = { 0x7f, 'E', 'L', 'F' };
		if (memcmp(data, elfMagic, sizeof(elfMagic)) == 0)
			objectFile = new ObjectFileElf();
		else
			objectFile = new ObjectFileCoff();

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

	bool ObjectFile::WriteSymbols(Logger& logger, MemoryBlock& memoryBlock)
	{
		auto write = [&](const void* data, u64 dataSize) { memcpy(memoryBlock.Allocate(dataSize, 1, TC("")), data, dataSize); };

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

	bool ObjectFile::WriteSymbols(Logger& logger, const tchar* exportsFilename)
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

	bool ObjectFile::CreateExtraFile(Logger& logger, const tchar* extraObjFilename, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile)
	{
		ObjectFileCoff objectFile;
		MemoryBlock memoryBlock(16*1024*1024);

		if (!((ObjectFile&)objectFile).CreateExtraFile(logger, memoryBlock, allNeededImports, allSharedImports, allSharedExports, includeExportsInFile))
			return false;

		FileAccessor extraFile(logger, extraObjFilename);
		if (!extraFile.CreateWrite())
			return false;

		if (!extraFile.Write(memoryBlock.memory, memoryBlock.writtenSize))
			return false;

		return extraFile.Close();
	}
}
