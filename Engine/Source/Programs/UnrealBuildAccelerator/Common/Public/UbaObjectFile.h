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

		bool CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& loopbacksToAdd);

		const tchar* GetFileName() const;
		const UnorderedSymbols& GetImports() const;
		const UnorderedSymbols& GetExports() const;
		const UnorderedSymbols& GetSymbols() const;

		~ObjectFile();

	private:
		FileAccessor* m_file = nullptr;

		u8* m_data = nullptr;
		u64 m_dataSize = 0;

		u32 m_sectionsMemOffset = 0;
		u32 m_sectionCount = 0;
		u64 m_directiveSectionMemOffset = 0;
		u8* m_stringTableMem = nullptr;
		u32 m_symbolsMemPos = 0;
		u32 m_symbolCount = 0;
		bool m_isBigObj = false;
		
		UnorderedSymbols m_imports;
		UnorderedSymbols m_exports;
		UnorderedSymbols m_symbols;

		void ParseExports();
		template<typename SymbolType> void ParseImports();


		void WriteExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports);
		template<typename SymbolType> void CalculateImports(Logger& logger, const UnorderedSymbols& loopbacksToAdd, Vector<u32>& outImports);
		template<typename SymbolType> void WriteImports(Logger& logger, u8* newData, const Vector<u32>& symbolsToAdd);
	};
}
