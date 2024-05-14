// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaLogger.h"
#include "UbaStringBuffer.h"

namespace uba
{
	class FileAccessor;
	
	using UnorderedSymbols = UnorderedSet<std::string>;

	class ObjectFile
	{
	public:
		bool Parse(Logger& logger, const tchar* filename);

		bool CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allExports);

		const tchar* GetFileName() const;
		const UnorderedSymbols& GetImports() const;
		const UnorderedSymbols& GetExports() const;

		~ObjectFile();

	private:
		FileAccessor* m_file = nullptr;

		u8* m_data = nullptr;
		u64 m_dataSize = 0;

		u8* m_sectionsMem = nullptr;
		u32 m_sectionCount = 0;
		u8* m_directiveSectionMem = nullptr;
		u8* m_stringTableMem = nullptr;
		u8* m_symbolsMem = nullptr;
		u32 m_symbolCount = 0;
		bool m_isBigObj = false;
		
		UnorderedSymbols m_imports;
		UnorderedSymbols m_exports;

		void ParseExports();
		template<typename SymbolType> void ParseImports();

		void WriteExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports);
		template<typename SymbolType> void WriteImports(Logger& logger, u8* newData, const UnorderedSymbols& allSharedExports);
	};
}
