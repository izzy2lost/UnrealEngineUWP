// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	class ObjectFileElf : public ObjectFile
	{
	public:
		virtual bool Parse(Logger& logger, const tchar* filename) override;
		virtual bool ComputeLoopbacksAndDuplicates(UnorderedExports& allSharedExports, UnorderedSymbols& duplicates) override;
		virtual bool CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports, u32& outKeptExportCount) override;

	private:
		virtual bool StripExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports, u32& outKeptExportCount) override;
		virtual bool CreateExtraFile(Logger& logger, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile) override;
		virtual bool CreateDefFile(Logger& logger, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedExports& allSharedExports) override;

		UnorderedSymbols m_toRemove;

		static UnorderedSymbols PotentiallyDuplicatedSymbols;
	};
}
