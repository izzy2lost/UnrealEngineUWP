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
		static ObjectFile* CreateAndParse(Logger& logger, const tchar* filename);

		virtual bool ComputeLoopbacksAndDuplicates(UnorderedSymbols& allSharedExports, UnorderedSymbols& duplicates) = 0;
		virtual bool CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports, u32& outKeptExportCount) = 0;

		const tchar* GetFileName() const;
		const UnorderedSymbols& GetImports() const;
		const UnorderedSymbols& GetExports() const;
		const UnorderedSymbols& GetPotentialDuplicates() const;

		virtual ~ObjectFile();

	protected:
		virtual bool Parse(Logger& logger, const tchar* filename) = 0;

		FileAccessor* m_file = nullptr;
		u8* m_data = nullptr;
		u64 m_dataSize = 0;

		UnorderedSymbols m_imports;
		UnorderedSymbols m_exports;
		UnorderedSymbols m_potentialDuplicates;
	};


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
	};
}
