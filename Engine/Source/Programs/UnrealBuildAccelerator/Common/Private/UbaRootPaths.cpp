// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaRootPaths.h"
#include "UbaFileAccessor.h"

#if PLATFORM_WINDOWS
#include <shlobj_core.h>
#endif

namespace uba
{
	bool RootPaths::RegisterRoot(Logger& logger, const tchar* rootPath, bool includeInKey)
	{
		// Register rootPath both with single path separators and double path separators on windows because text files store them with double path separators
		#if PLATFORM_WINDOWS
		StringBuffer<> doubleSlash;
		for (const tchar* it=rootPath; *it; ++it)
		{
			doubleSlash.Append(*it);
			if (*it == PathSeparator)
				doubleSlash.Append(PathSeparator);
		}

		const tchar* rootPaths[] = { rootPath, doubleSlash.data };
		#else
		const tchar* rootPaths[] = { rootPath };
		#endif

		for (const tchar* rp : rootPaths)
		{
			u32 index = u32(m_roots.size());
			if (index == '~' - ' ') // This is not really true.. as long as value is under 256 we're good
				return logger.Error(TC("Too many roots added (%llu)"), index);

			auto& root = m_roots.emplace_back();
			root.index = index;
			root.path = rp;

			ToLower(root.path.data());
			if (root.path[root.path.size()-1] != PathSeparator)
				return logger.Error(TC("Root path must end with separator"));

			root.includeInKey = includeInKey;

			m_longestRoot = Max(u32(root.path.size()), m_longestRoot);

			if (!m_shortestRoot || root.path.size() < m_shortestRoot)
			{
				m_shortestRoot = u32(root.path.size());
				for (auto& r : m_roots)
					r.shortestPathKey = ToStringKeyNoCheck(r.path.data(), m_shortestRoot);
			}
			else
				root.shortestPathKey = ToStringKeyNoCheck(root.path.data(), m_shortestRoot);
		}
		return true;
	}

	bool RootPaths::RegisterSystemRoots(Logger& logger)
	{
		#if PLATFORM_WINDOWS
		StringBuffer<MaxPath> dir;
		dir.count = GetSystemDirectory(dir.data, dir.capacity);
		RegisterRoot(logger, dir.EnsureEndsWithSlash().data, false); // Ignore files from here.. we do expect them not to affect the output of a process
		
		dir.count = GetEnvironmentVariable(TC("ProgramW6432"), dir.Clear().data, dir.capacity);
		RegisterRoot(logger, dir.EnsureEndsWithSlash().data, true);

		dir.count = GetEnvironmentVariable(TC("ProgramFiles(x86)"), dir.Clear().data, dir.capacity);
		RegisterRoot(logger, dir.EnsureEndsWithSlash().data, true);

		dir.count = GetEnvironmentVariable(TC("ProgramFiles(x86)"), dir.Clear().data, dir.capacity);
		RegisterRoot(logger, dir.EnsureEndsWithSlash().data, true);

		PWSTR path;
		if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &path)))
			return false;
		RegisterRoot(logger, dir.Clear().Append(path).EnsureEndsWithSlash().data, true);
		CoTaskMemFree(path);

		#else
		UBA_ASSERT(false);
		#endif
		return true;
	}

	const RootPaths::Root* RootPaths::FindRoot(const StringBufferBase& path) const
	{
		if (path.count < m_shortestRoot)
			return nullptr;

		StringBuffer<MaxPath> shortPath;
		shortPath.Append(path.data, m_shortestRoot).MakeLower();
		StringKey key = ToStringKeyNoCheck(shortPath.data, m_shortestRoot);
		for (u32 i=0, e=u32(m_roots.size()); i!=e; ++i)
		{
			auto& root = m_roots[i];
			if (key != root.shortestPathKey)
				continue;
			if (!path.StartsWith(root.path.c_str()))
				continue;
			return &m_roots[i];
		}
		return nullptr;
	}

	const RootPaths::Root& RootPaths::GetRoot(u32 index) const
	{
		return m_roots[index];
	}

	CasKey RootPaths::NormalizeAndHashFile(Logger& logger, const tchar* filename) const
	{
		FileAccessor file(logger, filename);
		if (!file.OpenMemoryRead())
			return CasKeyZero;

		CasKeyHasher hasher;
		auto hashString = [&](const char* str, u64 strLen, u32 rootPos) { hasher.Update(str, strLen); };
		if (!NormalizeString<char>(logger, (const char*)file.GetData(), file.GetSize(), hashString, filename))
			return CasKeyZero;

		return ToCasKey(hasher, false);
	}

}