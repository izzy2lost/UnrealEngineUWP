// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	// TODO: https://llvm.org/docs/BitCodeFormat.html

	class ObjectFileLLVMIR : public ObjectFile
	{
	public:
		virtual bool Parse(Logger& logger, const tchar* filename) override
		{
			return logger.Error(TC("LLVM IR obj file format not supported (yet)"));
		}

	private:
		virtual bool StripExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports, u32& outKeptExportCount) override { return false; }
		virtual bool CreateExtraFile(Logger& logger, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile) override { return false; }
	};


	bool IsLLVMIRFile(const u8* data, u64 dataSize)
	{
		constexpr u8 wrapperMagic[] = { 'B', 'C', 0xc0, 0xde };
		constexpr u8 magic[] = { 'B', 'C', 0x04, 0xc4, 0xe4, 0xd4 };
		return dataSize >= 6 && (memcmp(data, wrapperMagic, sizeof(wrapperMagic)) == 0 || memcmp(data, magic, sizeof(magic)) == 0);
	}
}
