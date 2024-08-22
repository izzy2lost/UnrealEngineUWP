// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	class ObjectFileElf : public ObjectFile
	{
	public:
		ObjectFileElf();
		virtual bool Parse(Logger& logger, const tchar* hint) override;

		static bool CreateExtraFile(Logger& logger, const StringView& platform, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile);

	private:
		virtual bool StripExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports) override;

		UnorderedSymbols m_toRemove;

		static UnorderedSymbols PotentiallyDuplicatedSymbols;
	};

	bool IsElfFile(const u8* data, u64 dataSize);
}
