// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	class ObjectFileImportLib : public ObjectFile
	{
	public:
		virtual bool Parse(Logger& logger, const tchar* hint) override;
		virtual const char* GetLibName() override;

	private:
		virtual bool StripExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports) override;
		virtual bool CreateExtraFile(Logger& logger, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile) override;

		std::string m_libName;
	};


	bool IsImportLib(const u8* data, u64 dataSize);
}
