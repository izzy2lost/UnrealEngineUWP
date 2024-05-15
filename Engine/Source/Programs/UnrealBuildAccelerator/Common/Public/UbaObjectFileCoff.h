// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	class ObjectFileCoff : public ObjectFile
	{
	public:
		virtual bool Parse(Logger& logger, const tchar* filename) override;
		virtual bool ComputeLoopbacksAndDuplicates(UnorderedSymbols& allSharedExports, UnorderedSymbols& duplicates) override;
		virtual bool CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports) override;

	private:
		struct Info;

		bool ParseExports();
		template<typename SymbolType> void ParseImports();

		void WriteExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports);
		template<typename SymbolType> void CalculateImports(Logger& logger, Vector<u32>& outImports);
		template<typename SymbolType> void WriteImports(Logger& logger, u8* newData, Info& newInfo, const Vector<u32>& symbolsToAdd);
		template<typename SymbolType> void RemoveSymbols(Logger& logger, u8* newData, Info& newInfo);

		struct Info
		{
			u32 sectionsMemOffset = 0;
			u32 sectionCount = 0;
			u64 directiveSectionMemOffset = 0;
			u8* stringTableMem = nullptr;
			u32 symbolsMemPos = 0;
			u32 symbolCount = 0;
		};

		bool m_isBigObj = false;
		Info m_info;

		UnorderedSymbols m_loopbacksToAdd;
		UnorderedSymbols m_toRemove;
	};
}
