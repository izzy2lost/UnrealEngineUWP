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

		bool CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports, UnorderedSymbols& loopbacksToAdd, UnorderedSymbols& toRemove);

		const tchar* GetFileName() const;
		const UnorderedSymbols& GetImports() const;
		const UnorderedSymbols& GetExports() const;
		const UnorderedSymbols& GetPotentialDuplicates() const;

		~ObjectFile();

	private:
		struct Info;

		void ParseExports();
		template<typename SymbolType> void ParseImports();

		void WriteExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports);
		template<typename SymbolType> void CalculateImports(Logger& logger, UnorderedSymbols& loopbacksToAdd, Vector<u32>& outImports);
		template<typename SymbolType> void WriteImports(Logger& logger, u8* newData, Info& newInfo, const Vector<u32>& symbolsToAdd);
		template<typename SymbolType> void RemoveSymbols(Logger& logger, u8* newData, Info& newInfo, UnorderedSymbols& toRemove);

		struct Info
		{
			u32 sectionsMemOffset = 0;
			u32 sectionCount = 0;
			u64 directiveSectionMemOffset = 0;
			u8* stringTableMem = nullptr;
			u32 symbolsMemPos = 0;
			u32 symbolCount = 0;
		};

		FileAccessor* m_file = nullptr;

		u8* m_data = nullptr;
		u64 m_dataSize = 0;

		bool m_isBigObj = false;
		Info m_info;

		UnorderedSymbols m_imports;
		UnorderedSymbols m_exports;
		UnorderedSymbols m_potentialDuplicates;
	};
}
