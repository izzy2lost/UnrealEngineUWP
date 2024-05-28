// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaCacheEntry.h"

namespace uba
{
	bool TestCacheEntry(Logger& logger, const StringBufferBase& rootDir)
	{
		CacheEntries entries;

		auto AddEntry = [&](const Set<u32>& inputs)
			{
				Vector<u8> inputOffsets;
				u64 bytes = 0;
				for (u32 i : inputs)
					bytes += Get7BitEncodedCount(i);
				inputOffsets.resize(bytes);
				BinaryWriter writer(inputOffsets.data(), 0, inputOffsets.size());
				for (u32 i : inputs)
					writer.Write7BitEncoded(i);

				CacheEntry entry;
				entries.BuildInputs(entry, inputs);
				entries.entries.push_back(entry);
				entries.ValidateEntry(logger, entry, inputOffsets);
			};

		AddEntry(Set<u32>{ 1, 4, 6 });
		AddEntry(Set<u32>{ 0, 4, 6 });
		AddEntry(Set<u32>{ 2, 4, 6 });
		AddEntry(Set<u32>{ 1, 4, 5 });
		AddEntry(Set<u32>{ 1, 4, 7 });
		AddEntry(Set<u32>{ 1, 3, 6 });
		AddEntry(Set<u32>{ 1, 5, 6 });
		AddEntry(Set<u32>{ 1, 4, 6, 7 });
		AddEntry(Set<u32>{ 0, 1, 4, 6 });

		return true;
	}
}