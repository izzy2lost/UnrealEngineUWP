// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCacheEntry.h"
#include "UbaFile.h"
#include <algorithm>

namespace uba
{
	u64 CacheEntries::GetSharedSize()
	{
		return sizeof(u16) + Get7BitEncodedCount(sharedInputCasKeyOffsets.size()) + sharedInputCasKeyOffsets.size();
	}

	u64 CacheEntries::GetEntrySize(CacheEntry& entry, bool toDisk)
	{
		u64 size = 0;

		if (toDisk)
			size += Get7BitEncodedCount(entry.creationTime) + Get7BitEncodedCount(entry.lastUsedTime);
		else
			size += Get7BitEncodedCount(entry.id);

		auto& extra = entry.extraInputCasKeyOffsets;
		size += Get7BitEncodedCount(extra.size()) + extra.size();

		auto& ranges = entry.sharedInputCasKeyOffsetRanges;
		size += Get7BitEncodedCount(ranges.size()) + ranges.size();

		auto& outputs = entry.outputCasKeyOffsets;
		size += Get7BitEncodedCount(outputs.size()) + outputs.size();
		return size;
	}

	u64 CacheEntries::GetTotalSize(bool toDisk)
	{
		u64 size = GetSharedSize();
		for (auto& entry : entries)
			size += GetEntrySize(entry, toDisk);
		return size;
	}

	bool CacheEntries::Write(BinaryWriter& writer, u32 clientVersion, bool toDisk)
	{
		u16& entryCount = *(u16*)writer.AllocWrite(2);
		entryCount = 0;

		if (clientVersion == 3)
		{
			UBA_ASSERT(!toDisk);

			Vector<u8> flattenInputs;
			for (auto& entry : entries)
			{
				Flatten(flattenInputs, entry);
				auto& inputs = flattenInputs;
				auto& outputs = entry.outputCasKeyOffsets;

				u64 neededSize = Get7BitEncodedCount(inputs.size()) + inputs.size() + Get7BitEncodedCount(outputs.size()) + outputs.size();
				if (neededSize > writer.GetCapacityLeft())
					return true;

				writer.Write7BitEncoded(inputs.size());
				writer.WriteBytes(inputs.data(), inputs.size());
				writer.Write7BitEncoded(outputs.size());
				writer.WriteBytes(outputs.data(), outputs.size());
				++entryCount;
			}
			return true;
		}

		{
			auto& shared = sharedInputCasKeyOffsets;

			if (!toDisk)
			{
				u64 neededSize = Get7BitEncodedCount(shared.size()) + shared.size();
				if (neededSize > writer.GetCapacityLeft())
					return true;
			}
			writer.Write7BitEncoded(shared.size());
			writer.WriteBytes(shared.data(), shared.size());
		}

		for (auto& entry : entries)
		{
			auto& extra = entry.extraInputCasKeyOffsets;
			auto& ranges = entry.sharedInputCasKeyOffsetRanges;
			auto& outputs = entry.outputCasKeyOffsets;

			if (toDisk)
			{
				writer.Write7BitEncoded(entry.creationTime);
				writer.Write7BitEncoded(entry.lastUsedTime);
			}
			else
			{
				u64 neededSize = Get7BitEncodedCount(entry.id) + Get7BitEncodedCount(extra.size()) + extra.size();
				neededSize += Get7BitEncodedCount(ranges.size()) + ranges.size();
				neededSize += Get7BitEncodedCount(outputs.size()) + outputs.size();

				if (neededSize > writer.GetCapacityLeft())
					return true;

				writer.Write7BitEncoded(entry.id);
			}

			writer.Write7BitEncoded(extra.size());
			writer.WriteBytes(extra.data(), extra.size());

			writer.Write7BitEncoded(ranges.size());
			writer.WriteBytes(ranges.data(), ranges.size());

			writer.Write7BitEncoded(outputs.size());
			writer.WriteBytes(outputs.data(), outputs.size());

			++entryCount;
		}

		return true;
	}

	bool CacheEntries::Read(Logger& logger, BinaryReader& reader, u32 databaseVersion)
	{
		if (databaseVersion == 3)
		{
			u32 cacheEntryCount = reader.ReadU32();
			Vector<u32> temp;
			while (cacheEntryCount--)
			{
				auto& cacheEntry = entries.emplace_back();
				cacheEntry.id = idCounter++;
				reader.ReadU64();
				cacheEntry.creationTime = GetSystemTimeAsFileTime();
				cacheEntry.lastUsedTime = GetSystemTimeAsFileTime();

				u32 inputSize = reader.ReadU32();
				#if UBA_USE_OLD
				cacheEntry.inputCasKeyOffsets.resize(inputSize);
				reader.ReadBytes(cacheEntry.inputCasKeyOffsets.data(), inputSize);
				reader.SetPosition(reader.GetPosition() - inputSize);
				#endif

				u64 inputEnd = reader.GetPosition() + inputSize;
				temp.clear();
				while (reader.GetPosition() < inputEnd)
					temp.push_back(u32(reader.Read7BitEncoded()));
				BuildInputsT(cacheEntry, temp, entries.size() == 1);

				u32 outputSize = reader.ReadU32();
				cacheEntry.outputCasKeyOffsets.resize(outputSize);
				reader.ReadBytes(cacheEntry.outputCasKeyOffsets.data(), outputSize);
			}

			#if UBA_USE_OLD
			ValidateEntries(logger);
			#endif
			return true;
		}

		u16 entryCount = reader.ReadU16();
		u64 sharedSize = reader.Read7BitEncoded();
		sharedInputCasKeyOffsets.resize(sharedSize);
		reader.ReadBytes(sharedInputCasKeyOffsets.data(), sharedSize);

		while (entryCount--)
		{
			auto& entry = entries.emplace_back();
			entry.id = idCounter++;
			entry.creationTime = reader.Read7BitEncoded();
			entry.lastUsedTime = reader.Read7BitEncoded();

			u64 extraSize = reader.Read7BitEncoded();
			entry.extraInputCasKeyOffsets.resize(extraSize);
			reader.ReadBytes(entry.extraInputCasKeyOffsets.data(), extraSize);

			u64 rangeSize = reader.Read7BitEncoded();
			entry.sharedInputCasKeyOffsetRanges.resize(rangeSize);
			reader.ReadBytes(entry.sharedInputCasKeyOffsetRanges.data(), rangeSize);

			u64 outputSize = reader.Read7BitEncoded();
			entry.outputCasKeyOffsets.resize(outputSize);
			reader.ReadBytes(entry.outputCasKeyOffsets.data(), outputSize);
		}

		return true;
	}

	template<typename Container>
	void CacheEntries::BuildInputsT(CacheEntry& entry, const Container& sortedInputs, bool populateShared)
	{
		StackBinaryWriter<256*1024> rangeWriter;

		auto g = MakeGuard([&]()
			{
				entry.sharedInputCasKeyOffsetRanges.resize(rangeWriter.GetPosition());
				memcpy(entry.sharedInputCasKeyOffsetRanges.data(), rangeWriter.GetData(), rangeWriter.GetPosition());
			});

		auto writeRange = [&](u64 begin, u64 end) { rangeWriter.Write7BitEncoded(begin); rangeWriter.Write7BitEncoded(end); };

		if (populateShared)
		{
			u64 bytes = 0;
			for (u32 i : sortedInputs)
				bytes += Get7BitEncodedCount(i);
			sharedInputCasKeyOffsets.resize(bytes);
			BinaryWriter writer(sharedInputCasKeyOffsets.data(), 0, sharedInputCasKeyOffsets.size());
			for (u32 i : sortedInputs)
				writer.Write7BitEncoded(i);
			UBA_ASSERT(bytes == writer.GetPosition());
			writeRange(0, bytes);
			return;
		}

		auto inputsIt = sortedInputs.begin();
		auto inputsEnd = sortedInputs.end();

		BinaryReader sharedReader(sharedInputCasKeyOffsets.data(), 0, sharedInputCasKeyOffsets.size());

		u32 sharedOffset = ~0u;
		u32 offset = ~0u;

		u32 rangeBegin = 0;
		bool inRange = false;
		bool unhandledOffset = false;
		u32 lastSharedPos = ~0u;

		Vector<u8> extraOffsets;
		extraOffsets.resize(sortedInputs.size()*4);
		BinaryWriter extraWriter(extraOffsets.data(), 0, extraOffsets.size());

		while (true)
		{
			u32 sharedPos = u32(sharedReader.GetPosition());

			if (!sharedReader.GetLeft())
			{
				// Add current range if there is one going
				if (inRange)
					writeRange(rangeBegin, sharedPos);

				if (offset > sharedOffset)
					extraWriter.Write7BitEncoded(offset);

				// Populate rest in extraInputCasKeyOffsets
				for (;inputsIt != inputsEnd ;++inputsIt)
					extraWriter.Write7BitEncoded(*inputsIt);
				break;
			}

			if (inputsIt == inputsEnd)
			{
				// Add current range if there is one going
				if (inRange)
					writeRange(rangeBegin, sharedPos);
				if (offset > sharedOffset)
					extraWriter.Write7BitEncoded(offset);
				break;
			}

			if (sharedOffset < offset)
			{
				lastSharedPos = sharedPos;
				sharedOffset = u32(sharedReader.Read7BitEncoded());
			}
			else if (offset < sharedOffset)
			{
				offset = *inputsIt++;
				sharedPos = lastSharedPos;
			}
			else
			{
				lastSharedPos = sharedPos;
				sharedOffset = u32(sharedReader.Read7BitEncoded());
				offset = *inputsIt++;
			}

			if (sharedOffset == offset)
			{
				if (!inRange)
				{
					rangeBegin = sharedPos;
					inRange = true;
				}
			}
			else
			{
				if (inRange)
				{
					inRange = false;
					writeRange(rangeBegin, sharedPos);
				}
				if (offset < sharedOffset)
				{
					extraWriter.Write7BitEncoded(offset);
				}
				else
				{
					unhandledOffset = true;
				}
			}
		}

		entry.extraInputCasKeyOffsets.resize(extraWriter.GetPosition());
		memcpy(entry.extraInputCasKeyOffsets.data(), extraWriter.GetData(), extraWriter.GetPosition());
	}

	void CacheEntries::BuildInputs(CacheEntry& entry, const Set<u32>& inputs)
	{
		BuildInputsT(entry, inputs, entries.empty());
	}

	void CacheEntries::UpdateEntries()
	{
		if (entries.empty())
			return;

		// Checking if first entry is still the matching entry
		auto& firstEntry = *entries.begin();
		if (firstEntry.extraInputCasKeyOffsets.empty())
		{
			BinaryReader reader(firstEntry.sharedInputCasKeyOffsetRanges.data(), 0, firstEntry.sharedInputCasKeyOffsetRanges.size());
			u64 begin = reader.Read7BitEncoded();
			u64 end = reader.Read7BitEncoded();
			if (begin == 0 && end == sharedInputCasKeyOffsets.size())
				return;
		}

		// First entry is gone, need to refresh the shared table...


		Vector<u8> oldShared;
		bool isFirst = true;
		Vector<u32> temp;
		for (auto& entry : entries)
		{
			if (isFirst)
			{
				// Flatten first entry into new shared
				Flatten(oldShared, entry);
				oldShared.swap(sharedInputCasKeyOffsets);
				entry.extraInputCasKeyOffsets.clear();
				entry.sharedInputCasKeyOffsetRanges.resize(1 + Get7BitEncodedCount(sharedInputCasKeyOffsets.size()));
				BinaryWriter rangeWriter(entry.sharedInputCasKeyOffsetRanges.data(), 0, entry.sharedInputCasKeyOffsetRanges.size());
				rangeWriter.Write7BitEncoded(0);
				rangeWriter.Write7BitEncoded(sharedInputCasKeyOffsets.size());
				isFirst = false;
			}
			else
			{
				// Flatten using old shared and rebuild it with new shared
				Flatten(temp, entry, oldShared);
				entry.extraInputCasKeyOffsets.clear();
				entry.sharedInputCasKeyOffsetRanges.clear();
				BuildInputsT(entry, temp, false);
			}
		}
	}

	void CacheEntries::UpdateEntries(Logger& logger, const GrowingNoLockUnorderedMap<u32, u32>& oldToNewCasKeyOffset, Vector<u32>& temp)
	{
		if (entries.empty())
			return;

		#if UBA_USE_OLD
		ValidateEntries(logger);
		#endif

		Vector<u8> oldShared;
		oldShared.swap(sharedInputCasKeyOffsets);

		bool isFirst = true;
		for (auto& entry : entries)
		{
			if (isFirst)
			{
				// Flatten first entry into temp
				Flatten(temp, entry, oldShared);

				// Update temp with new offsets
				u64 newSize = 0;
				for (auto& offset : temp)
				{
					auto findIt = oldToNewCasKeyOffset.find(offset);
					if (findIt != oldToNewCasKeyOffset.end())
						offset = findIt->second;
					newSize += Get7BitEncodedCount(offset);
				}

				// Sort temp now when it likely is out of order
				std::sort(temp.begin(), temp.end());

				sharedInputCasKeyOffsets.resize(newSize);
				BinaryWriter writer(sharedInputCasKeyOffsets.data(), 0, newSize);
				for (auto& offset : temp)
					writer.Write7BitEncoded(offset);

				entry.extraInputCasKeyOffsets.clear();
				u64 rangeSize = 1 + Get7BitEncodedCount(newSize);
				entry.sharedInputCasKeyOffsetRanges.resize(rangeSize);
				BinaryWriter rangeWriter(entry.sharedInputCasKeyOffsetRanges.data(), 0, rangeSize);
				rangeWriter.Write7BitEncoded(0);
				rangeWriter.Write7BitEncoded(newSize);
				UBA_ASSERT(rangeWriter.GetPosition() == rangeSize);
				isFirst = false;
			}
			else
			{
				// Flatten using old shared and rebuild it with new shared
				Flatten(temp, entry, oldShared);
				for (auto& offset : temp)
				{
					auto findIt = oldToNewCasKeyOffset.find(offset);
					if (findIt != oldToNewCasKeyOffset.end())
						offset = findIt->second;
				}

				// Sort temp now when it likely is out of order
				std::sort(temp.begin(), temp.end());

				entry.extraInputCasKeyOffsets.clear();
				entry.sharedInputCasKeyOffsetRanges.clear();
				BuildInputsT(entry, temp, false);
			}
		}

		auto updateCasKeyOffsets = [&](Vector<u8>& offsets)
			{
				temp.clear();
				temp.reserve(offsets.size()*4);

				u32 newOffsetsSize = 0;
				BinaryReader reader2(offsets.data(), 0, offsets.size());
				while (reader2.GetLeft())
				{
					u32 oldOffset = u32(reader2.Read7BitEncoded());
					u32 newOffset = oldOffset;
					auto findIt = oldToNewCasKeyOffset.find(oldOffset);
					if (findIt != oldToNewCasKeyOffset.end())
						newOffset = findIt->second;
					temp.push_back(newOffset);
					newOffsetsSize += Get7BitEncodedCount(newOffset);
				}

				std::sort(temp.begin(), temp.end());

				offsets.resize(newOffsetsSize);
				BinaryWriter writer2(offsets.data(), 0, newOffsetsSize);
				for (u32 offset : temp)
					writer2.Write7BitEncoded(offset);
				UBA_ASSERT(writer2.GetPosition() == newOffsetsSize);
			};

		for (auto& entry : entries)
		{
			#if UBA_USE_OLD
			updateCasKeyOffsets(entry.inputCasKeyOffsets);
			#endif
			updateCasKeyOffsets(entry.outputCasKeyOffsets);
		}

		#if UBA_USE_OLD
		ValidateEntries(logger);
		#endif
	}

	#if UBA_USE_OLD
	void CacheEntries::ValidateEntries(Logger& logger)
	{
		for (auto& entry : entries)
			ValidateEntry(logger, entry);
	}
	void CacheEntries::ValidateEntry(Logger& logger, CacheEntry& entry)
	{
		Vector<u8> res;
		Flatten(res, entry);
		if (res.size() == entry.inputCasKeyOffsets.size() && memcmp(res.data(), entry.inputCasKeyOffsets.data(), res.size()) == 0)
			return;
		BinaryReader reader1(entry.inputCasKeyOffsets.data(), 0, entry.inputCasKeyOffsets.size());
		BinaryReader reader2(res.data(), 0, res.size());
		while (reader1.GetLeft() || reader2.GetLeft())
		{
			u32 a = ~0u;
			u32 b = ~0u;
			if (reader1.GetLeft())
				a = u32(reader1.Read7BitEncoded());
			if (reader2.GetLeft())
				b = u32(reader2.Read7BitEncoded());

			logger.Detail(TC("A: %u B: %u"), a, b);
		}
		UBA_ASSERT(false);
	}

	#endif

	void CacheEntries::Flatten(Vector<u8>& out, const CacheEntry& entry)
	{
		u64 size = entry.extraInputCasKeyOffsets.size();
		{
			BinaryReader rangeReader(entry.sharedInputCasKeyOffsetRanges.data(), 0, entry.sharedInputCasKeyOffsetRanges.size());
			while (rangeReader.GetLeft())
			{
				u64 begin = rangeReader.Read7BitEncoded();
				u64 end = rangeReader.Read7BitEncoded();
				size += end - begin;
			}
		}

		out.resize(size);
		BinaryWriter writer(out.data(), 0, out.size());

		BinaryReader extraReader(entry.extraInputCasKeyOffsets.data(), 0, entry.extraInputCasKeyOffsets.size());
		u32 nextExtra = ~0u;
		if (extraReader.GetLeft())
			nextExtra = u32(extraReader.Read7BitEncoded());

		auto writeExtra = [&](u32 prevOffset)
			{
				while (nextExtra < prevOffset)
				{
					writer.Write7BitEncoded(nextExtra);
					nextExtra = ~0u;
					if (extraReader.GetLeft())
						nextExtra = u32(extraReader.Read7BitEncoded());
				};
			};

		BinaryReader rangeReader(entry.sharedInputCasKeyOffsetRanges.data(), 0, entry.sharedInputCasKeyOffsetRanges.size());
		while (rangeReader.GetLeft())
		{
			u64 begin = rangeReader.Read7BitEncoded();
			u64 end = rangeReader.Read7BitEncoded();
			BinaryReader inputReader(sharedInputCasKeyOffsets.data() + begin, 0, end - begin);
			while (inputReader.GetLeft())
			{
				u32 offset = u32(inputReader.Read7BitEncoded());
				writeExtra(offset);
				writer.Write7BitEncoded(offset);
			}
		}

		writeExtra(~0u);
	}

	void CacheEntries::Flatten(Vector<u32>& out, const CacheEntry& entry, const Vector<u8>& sharedOffsets)
	{
		out.clear();

		BinaryReader extraReader(entry.extraInputCasKeyOffsets.data(), 0, entry.extraInputCasKeyOffsets.size());
		u32 nextExtra = ~0u;
		if (extraReader.GetLeft())
			nextExtra = u32(extraReader.Read7BitEncoded());

		auto writeExtra = [&](u32 prevOffset)
			{
				while (nextExtra < prevOffset)
				{
					out.push_back(nextExtra);
					nextExtra = ~0u;
					if (extraReader.GetLeft())
						nextExtra = u32(extraReader.Read7BitEncoded());
				};
			};

		BinaryReader rangeReader(entry.sharedInputCasKeyOffsetRanges.data(), 0, entry.sharedInputCasKeyOffsetRanges.size());
		while (rangeReader.GetLeft())
		{
			u64 begin = rangeReader.Read7BitEncoded();
			u64 end = rangeReader.Read7BitEncoded();
			BinaryReader inputReader(sharedOffsets.data() + begin, 0, end - begin);
			while (inputReader.GetLeft())
			{
				u32 offset = u32(inputReader.Read7BitEncoded());
				writeExtra(offset);
				out.push_back(offset);
			}
		}

		writeExtra(~0u);
	}
}
