// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	class ObjectFileLLVMIR : public ObjectFile
	{
	public:
		ObjectFileLLVMIR();
		~ObjectFileLLVMIR();

		virtual bool Parse(Logger& logger, const tchar* filename) override;

	private:
		virtual bool StripExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports) override;
		virtual bool CreateExtraFile(Logger& logger, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile) override;

		enum BlockInfoCodes : u8;
		enum Encoding : u8;
		enum EntryKind : u8;
		enum FixedAbbrevIDs : u8;
		struct Abbrev;
		struct AbbrevOp;
		struct BlockInfo;
		struct Entry;
		using AbbrevPtr = std::shared_ptr<Abbrev>;

		class BitStreamReader;
		bool ParseBlock(Logger& logger, BitStreamReader& reader, BlockInfo& blockInfo, u32 blockId, u32 indent);

		struct BitStreamEntry
		{
			u64 pos;
			u32 word;
			u32 wordBits;
			u32 code;
			bool isExport;
			Vector<AbbrevOp> operands;
		};

		Vector<BitStreamEntry> m_globalVarOrFunctionRecords;

		u64 m_strTabPos;
		u64 m_strTabSize;
		//std::string m_strTab;
	};


	bool IsLLVMIRFile(const u8* data, u64 dataSize);
}
