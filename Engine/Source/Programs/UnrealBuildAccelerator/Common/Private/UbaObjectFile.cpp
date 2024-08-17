// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFile.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaCompressedObjFileHeader.h"
#include "UbaFileAccessor.h"
#include "UbaObjectFileCoff.h"
#include "UbaObjectFileElf.h"
#include "UbaObjectFileImportLib.h"
#include "UbaObjectFileLLVMIR.h"
#include <oodle2.h>

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

		bool ownsData = false;
		if (dataSize >= sizeof(CompressedObjFileHeader) && ((CompressedObjFileHeader*)data)->IsValid())
		{
			u64 decompressedSize = *(u64*)(data + sizeof(CompressedObjFileHeader));
			u8* readPos = data + sizeof(CompressedObjFileHeader) + 8;

			u8* decompressedData = (u8*)malloc(decompressedSize);
			u8* writePos = decompressedData;

			OO_SINTa decoredMemSize = OodleLZDecoder_MemorySizeNeeded(OodleLZ_Compressor_Kraken);
			void* decoderMem = malloc(decoredMemSize);
			auto mg = MakeGuard([decoderMem]() { free(decoderMem); });

			u64 left = decompressedSize;
			while (left)
			{
				u32 compressedBlockSize = *(u32*)readPos;
				readPos += 4;
				u32 decompressedBlockSize = *(u32*)readPos;
				readPos += 4;

				OO_SINTa decompLen = OodleLZ_Decompress(readPos, (OO_SINTa)compressedBlockSize, writePos, (OO_SINTa)decompressedBlockSize,
					OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC_No, OodleLZ_Verbosity_None, NULL, 0, NULL, NULL, decoderMem, decoredMemSize);
				if (decompLen != decompressedBlockSize)
				{
					logger.Error(TC("Failed to decompress file %s"), hint);
					return nullptr;
				}

				readPos += compressedBlockSize;
				writePos += decompressedBlockSize;
				left -= decompressedBlockSize;
			}

			data = decompressedData;
			dataSize = decompressedSize;
			ownsData = true;
		}

		if (IsElfFile(data, dataSize))
			objectFile = new ObjectFileElf();
		else if (IsLLVMIRFile(data, dataSize))
			objectFile = new ObjectFileLLVMIR();
		else if (IsCoffFile(data, dataSize))
			objectFile = new ObjectFileCoff();
		else if (IsImportLib(data, dataSize))
			objectFile = new ObjectFileImportLib();
		else
		{
			if (ownsData)
				free(data);
			logger.Error(TC("Unknown object file format. Maybe msvc FE IL? (%s)"), hint);
			return nullptr;
		}

		objectFile->m_data = data;
		objectFile->m_dataSize = dataSize;
		objectFile->m_ownsData = ownsData;

		if (objectFile->Parse(logger, hint))
			return objectFile;

		if (ownsData)
			free(data);
		delete objectFile;
		return nullptr;
	}

	bool ObjectFile::CopyMemoryAndClose()
	{
		u8* data = (u8*)malloc(m_dataSize);
		memcpy(data, m_data, m_dataSize);
		if (m_ownsData)
			free(m_data);
		m_data = data;
		m_ownsData = true;
		delete m_file;
		m_file = nullptr;
		return true;
	}

	bool ObjectFile::StripExports(Logger& logger)
	{
		return StripExports(logger, m_data, {});
	}

	bool ObjectFile::WriteImportsAndExports(Logger& logger, MemoryBlock& memoryBlock)
	{
		auto write = [&](const void* data, u64 dataSize) { memcpy(memoryBlock.Allocate(dataSize, 1, TC("ObjectFile::WriteImportsAndExports")), data, dataSize); };

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
			write(kv.second.extra.c_str(), kv.second.extra.size());
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
			write(kv.second.extra.c_str(), kv.second.extra.size());
			write("", 1);
		}
		write("", 1);

		flush();

		return exportsFile.Close();
	}

	const char* ObjectFile::GetLibName()
	{
		UBA_ASSERT(false);
		return "";
	}

	ObjectFile::~ObjectFile()
	{
		if (m_ownsData)
			free(m_data);
		delete m_file;
	}

	void ObjectFile::RemoveExportedSymbol(const char* symbol)
	{
		m_exports.erase(symbol);
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
		ObjectFileLLVMIR objectFileLLVMIR;
		
		ObjectFile& objectFile = [&]() -> ObjectFile&
			{
				switch (type)
				{
				case ObjectFileType_Coff:
					return objectFileCoff;
				case ObjectFileType_Elf:
					return objectFileCoff;
				case ObjectFileType_LLVMIR:
					return objectFileLLVMIR;
				default:
					UBA_ASSERT(false);
					return objectFileCoff;
				}
			}();


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
			ExportInfo info;
			if (const char* comma = strchr(readPos, ','))
			{
				strEnd = comma - readPos;
				info.extra = comma;
			}
			exports.emplace(std::string(readPos, readPos + strEnd), info);
			readPos = readPos + strEnd + 1;
		}
		return true;
	}
}
