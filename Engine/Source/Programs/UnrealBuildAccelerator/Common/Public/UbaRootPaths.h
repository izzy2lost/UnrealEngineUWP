// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaHash.h"
#include "UbaLogger.h"

namespace uba
{
	class RootPaths
	{
	public:

		bool RegisterRoot(Logger& logger, const tchar* rootPath, bool includeInKey = true);
		bool RegisterSystemRoots(Logger& logger);

		struct Root
		{
			TString path;
			StringKey shortestPathKey;
			u32 index;
			bool includeInKey;
		};

		const Root* FindRoot(const StringBufferBase& path) const;
		const Root& GetRoot(u32 index) const;

		template<typename CharType, typename Func>
		bool NormalizeString(Logger& logger, const CharType* str, u64 strLen, const Func& func, const tchar* hint) const;

		CasKey NormalizeAndHashFile(Logger& logger, const tchar* filename) const;

		static constexpr u8 RootStartByte = ' ';

	private:
		Vector<Root> m_roots;
		u32 m_shortestRoot = 0;
		u32 m_longestRoot = 0;
	};



	template<typename CharType, typename Func>
	bool RootPaths::NormalizeString(Logger& logger, const CharType* str, u64 strLen, const Func& func, const tchar* hint) const
	{
		auto strEnd = str + strLen;
		auto searchPos = str;

		u32 destPos = 0;

		while (true)
		{
			auto absPathChars = searchPos;
			CharType lastChar = 0;
			while (absPathChars < strEnd && !(lastChar == ':' && *absPathChars == '\\'))
			{
				lastChar = *absPathChars;
				++absPathChars;
			}
		
			if (absPathChars == strEnd)
			{
				func(searchPos, strEnd - searchPos, ~0u);
				return true;
			}

			auto pathStart = absPathChars - 2;

			auto pathEndOrMore = pathStart;
			while (pathEndOrMore < strEnd && *pathEndOrMore != '\n')
				++pathEndOrMore;

			u32 lenOrMore = u32(pathEndOrMore - pathStart);
			u32 toCopy = Min(lenOrMore, m_longestRoot);
			StringBuffer<512> path;
			path.Append(pathStart, toCopy);

			auto root = FindRoot(path);
			if (!root)
			{
				logger.Info(TC("PATH WITHOUT ROOT: %s (inside file %s)"), path.data, hint);
				return false;
			}

			if (u32 len = u32(pathStart - searchPos))
			{
				destPos += len;
				func(searchPos, len, ~0u);
			}
			CharType temp = RootStartByte + CharType(root->index);
			func(&temp, 1, destPos);
			destPos += 1;

			searchPos = pathStart + root->path.size();
		}
	}
}
