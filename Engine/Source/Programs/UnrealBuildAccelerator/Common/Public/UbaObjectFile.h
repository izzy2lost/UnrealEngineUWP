// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaLogger.h"
#include "UbaStringBuffer.h"

namespace uba
{
	class FileAccessor;
	
	using UnorderedSymbols = UnorderedSet<std::string>;
	using UnorderedExports = UnorderedMap<std::string, std::string>;

	enum ObjectFileType : u8
	{
		ObjectFileType_Unknown,
		ObjectFileType_Coff,
		ObjectFileType_Elf,
	};

	class ObjectFile
	{
	public:
		static ObjectFile* OpenAndParse(Logger& logger, const tchar* filename);
		static ObjectFile* Parse(Logger& logger, u8* data, u64 dataSize, const tchar* hint);

		virtual bool CopyMemoryAndClose();
		virtual bool StripExports(Logger& logger);
		virtual bool WriteImportsAndExports(Logger& logger, MemoryBlock& memoryBlock);
		virtual bool WriteImportsAndExports(Logger& logger, const tchar* exportsFilename);

		const tchar* GetFileName() const;
		const UnorderedSymbols& GetImports() const;
		const UnorderedExports& GetExports() const;
		const UnorderedSymbols& GetPotentialDuplicates() const;

		static bool CreateExtraFile(Logger& logger, const tchar* extraObjFilename, ObjectFileType type, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile);

		virtual ~ObjectFile();

	protected:
		virtual bool Parse(Logger& logger, const tchar* filename) = 0;
		virtual bool StripExports(Logger& logger, u8* newData, const UnorderedSymbols& allNeededImports, u32& outKeptExportCount) = 0;
		virtual bool CreateExtraFile(Logger& logger, MemoryBlock& memoryBlock, const UnorderedSymbols& allNeededImports, const UnorderedSymbols& allSharedImports, const UnorderedExports& allSharedExports, bool includeExportsInFile) = 0;

		FileAccessor* m_file = nullptr;
		u8* m_data = nullptr;
		u64 m_dataSize = 0;
		bool m_ownsData = false;

		ObjectFileType m_type;
		UnorderedSymbols m_imports;
		UnorderedExports m_exports;
		UnorderedSymbols m_potentialDuplicates;

	public:
		struct StringView
		{
			const char* strBegin;
			const char* strEnd;

			u32 Length() const
			{
				return u32(strEnd - strBegin);
			}

			bool StartsWith(const char* str, u32 strLen) const
			{
				if (strLen > Length())
					return false;
				return memcmp(strBegin, str, strLen) == 0;
			}

			bool Contains(const char* str, u32 strLen) const
			{
				const char* it = strBegin;
				const char* itEnd = strEnd - strLen + 1;
				while (it < itEnd)
				{
					if (memcmp(it, str, strLen) == 0)
						return true;
					++it;
				}
				return false;
			}

			bool Equals(const char* str, u32 strLen) const
			{
				if (strLen != Length())
					return false;
				return memcmp(strBegin, str, strLen) == 0;
			}

			std::string ToString() const
			{
				return std::string(strBegin, strEnd);
			}

			std::string& ToString(std::string& out) const
			{
				out.assign(strBegin, strEnd);
				return out;
			}
		};
	};

	struct SymbolFile
	{
		UnorderedSymbols imports;
		UnorderedExports exports;
		ObjectFileType type;

		bool ParseFile(Logger& logger, const tchar* filename);
	};
}
