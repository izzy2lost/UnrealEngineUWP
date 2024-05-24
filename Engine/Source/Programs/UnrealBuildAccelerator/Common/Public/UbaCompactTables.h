// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaHash.h"
#include "UbaMemory.h"

namespace uba
{
	struct BinaryReader;
	struct BinaryWriter;
	struct StringKey;

	class CompactPathTable
	{
	public:
		enum Version : u8 { V0, V1 };

		CompactPathTable(u64 reserveSize, Version version, bool caseSensitive, u64 reservePathCount = 0, u64 reserveSegmentCount = 0);

		u32 Add(const tchar* str, u64 strLen, u32* outRequiredCasTableSize = nullptr);
		u32 AddNoLock(const tchar* str, u64 strLen);

		void GetString(StringBufferBase& out, u64 offset) const;

		u8* GetMemory();
		u32 GetSize();

		void ReadMem(BinaryReader& reader, bool populateLookup);
		void Swap(CompactPathTable& other);

		u64 GetPathCount() { return m_offsets.size(); }
		u64 GetSegmentCount() { return m_segmentOffsets.size(); }

	private:
		u32 InternalAdd(const tchar* str, const tchar* stringKeyString, u64 strLen);
		ReaderWriterLock m_lock;
		MemoryBlock m_mem;
		UnorderedMap<StringKey, u32> m_offsets;
		UnorderedMap<StringKey, u32> m_segmentOffsets;
		u64 m_reserveSize;
		Version m_version;
		bool m_caseInsensitive;
	};

	class CompactCasKeyTable
	{
	public:
		CompactCasKeyTable(u64 reserveSize, u64 reserveOffsetsCount = 0);

		u32 Add(const CasKey& casKey, u64 stringOffset, u32* outRequiredCasTableSize = nullptr);

		void GetKey(CasKey& outKey, u64 offset) const;
		void GetPathAndKey(StringBufferBase& outPath, CasKey& outKey, const CompactPathTable& pathTable, u64 offset) const;

		u8* GetMemory();
		u32 GetSize();
		ReaderWriterLock& GetLock() { return m_lock; }

		void ReadMem(BinaryReader& reader, bool populateLookup);
		void Swap(CompactCasKeyTable& other);

		u64 GetKeyCount() { return m_offsets.size(); }

	private:
		ReaderWriterLock m_lock;
		MemoryBlock m_mem;
		struct Key
		{
			bool operator==(const Key& o) const { return ck == o.ck && offset == o.offset; }
			CasKey ck; u32 offset;
		};
		struct KeyHash { size_t operator()(const Key& k) const { return k.ck.a ^ k.ck.b ^ k.ck.c ^ k.offset; } };
		UnorderedMap<Key, u32, KeyHash> m_offsets;
		u64 m_reserveSize;
	};
}
