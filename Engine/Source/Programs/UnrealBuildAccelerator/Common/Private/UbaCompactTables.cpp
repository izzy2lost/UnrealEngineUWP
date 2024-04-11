// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCompactTables.h"
#include "UbaBinaryReaderWriter.h"

namespace uba
{
	CompactPathTable::CompactPathTable(u64 reserveSize, u64 reserveOffsetsCount)
	{
		m_reserveSize = reserveSize;
		if (reserveOffsetsCount)
			m_offsets.reserve(reserveOffsetsCount);
	}

	u32 CompactPathTable::Add(const tchar* str, u64 strLen, u32* outRequiredCasTableSize)
	{
		SCOPED_WRITE_LOCK(m_lock, lock);
		u32 res = AddNoLock(str, strLen);
		if (outRequiredCasTableSize)
			*outRequiredCasTableSize = u32(m_mem.writtenSize);
		return res;
	}

	u32 CompactPathTable::AddNoLock(const tchar* str, u64 strLen)
	{
		if (!m_mem.memory)
			m_mem.Init(m_reserveSize);
		if (!m_mem.writtenSize)
			m_mem.AllocateNoLock(1, 1, TC(""));

		const tchar* stringKeyString = str;
		StringBuffer<MaxPath> tempStringKeyStr;
		if (CaseInsensitiveFs)
			stringKeyString = tempStringKeyStr.Append(str).MakeLower().data;

		return InternalAdd(str, stringKeyString, strLen);
	}

	u32 CompactPathTable::InternalAdd(const tchar* str, const tchar* stringKeyString, u64 strLen)
	{
		StringKey key = ToStringKey(stringKeyString, strLen);
		auto insres = m_offsets.try_emplace(key);
		if (!insres.second)
			return insres.first->second;
			
		const tchar* seg = str;
		u32 parentOffset = 0;
		
		for (const tchar* it = str + strLen - 1; it > str; --it)
		{
			if (*it != PathSeparator)
				continue;

			parentOffset = InternalAdd(str, stringKeyString, it - str);
			seg = it + 1;
			break;
		}

		u64 segLen = strLen - (seg - str);
		u8 bytesForParent = Get7BitEncodedCount(parentOffset);
		u64 bytesForString = GetStringWriteSize(seg, segLen);

		u64 memSize = bytesForParent + bytesForString;
		u8* mem = (u8*)m_mem.AllocateNoLock(memSize, 1, TC(""));
		BinaryWriter writer(mem, 0, memSize);
		writer.Write7BitEncoded(parentOffset);
		writer.WriteString(seg, segLen);
		insres.first->second = u32(mem - m_mem.memory);
		return insres.first->second;
	}

	void CompactPathTable::GetString(StringBufferBase& out, u64 offset) const
	{
		u32 offsets[256];
		offsets[0] = u32(offset);
		u32 offsetCount = 0;

		BinaryReader reader(m_mem.memory, offset, m_mem.writtenSize);

		while (offset)
		{
			++offsetCount;
			UBA_ASSERT(offsetCount < sizeof_array(offsets));
			reader.SetPosition(offset);
			offset = (u32)reader.Read7BitEncoded();
			offsets[offsetCount] = u32(offset);
		}

		bool isFirst = true;
		for (u32 i=offsetCount;i; --i)
		{
			reader.SetPosition(offsets[i-1]);
			reader.Read7BitEncoded();

			if (!isFirst)
				out.Append(PathSeparator);
			isFirst = false;
			reader.ReadString(out);
		}
	}

	u8* CompactPathTable::GetMemory()
	{
		return m_mem.memory;
	}

	u32 CompactPathTable::GetSize()
	{
		SCOPED_READ_LOCK(m_lock, lock2)
		return u32(m_mem.writtenSize);
	}

	void CompactPathTable::ReadMem(BinaryReader& reader, bool populateLookup)
	{
		if (!m_mem.memory)
			m_mem.Init(m_reserveSize);

		u64 writtenSize = m_mem.writtenSize;
		u64 left = reader.GetLeft();
		void* mem = m_mem.AllocateNoLock(left, 1, TC(""));
		reader.ReadBytes(mem, left);

		if (!populateLookup)
			return;

		BinaryReader reader2(m_mem.memory, writtenSize, m_mem.writtenSize);
		if (!writtenSize)
			reader2.Skip(1);

		while (reader2.GetLeft())
		{
			u32 offset = u32(reader2.GetPosition());
			reader2.Read7BitEncoded();
			reader2.SkipString();
			StringBuffer<> str;
			GetString(str, offset);
			if (CaseInsensitiveFs)
				str.MakeLower();
			m_offsets.try_emplace(ToStringKey(str), offset);
		}
	}

	void CompactPathTable::Swap(CompactPathTable& other)
	{
		m_offsets.swap(other.m_offsets);
		m_mem.Swap(other.m_mem);
		u64 rs = m_reserveSize;
		m_reserveSize = other.m_reserveSize;
		other.m_reserveSize = rs;
	}

	CompactCasKeyTable::CompactCasKeyTable(u64 reserveSize, u64 reserveOffsetsCount)
	{
		m_reserveSize = reserveSize;
		if (reserveOffsetsCount)
			m_offsets.reserve(reserveOffsetsCount);
	}

	u32 CompactCasKeyTable::Add(const CasKey& casKey, u64 stringOffset, u32* outRequiredCasTableSize)
	{
		SCOPED_WRITE_LOCK(m_lock, lock2)
		if (!m_mem.memory)
			m_mem.Init(m_reserveSize);
		auto insres = m_offsets.try_emplace({ casKey, u32(stringOffset)});
		if (insres.second)
		{
			u8 bytesForStringOffset = Get7BitEncodedCount(stringOffset);
			u8* mem = (u8*)m_mem.AllocateNoLock(bytesForStringOffset + sizeof(CasKey), 1, TC(""));
			BinaryWriter writer(mem, 0, 1000);
			writer.Write7BitEncoded(stringOffset);
			writer.WriteCasKey(casKey);
			insres.first->second = u32(mem - m_mem.memory);
			if (outRequiredCasTableSize)
				*outRequiredCasTableSize = (u32)m_mem.writtenSize;
		}
		else if (outRequiredCasTableSize)
		{
			BinaryReader reader(m_mem.memory, insres.first->second, 1000);
			reader.Read7BitEncoded();
			*outRequiredCasTableSize = Max(*outRequiredCasTableSize, u32(reader.GetPosition() + sizeof(CasKey)));
		}
		return insres.first->second;
	}

	void CompactCasKeyTable::GetKey(CasKey& outKey, u64 offset) const
	{
		BinaryReader reader(m_mem.memory, offset, 1000);
		reader.Read7BitEncoded();
		outKey = reader.ReadCasKey();
	}

	void CompactCasKeyTable::GetPathAndKey(StringBufferBase& outPath, CasKey& outKey, const CompactPathTable& pathTable, u64 offset) const
	{
		BinaryReader reader(m_mem.memory, offset, 1000);
		u32 stringOffset = (u32)reader.Read7BitEncoded();
		outKey = reader.ReadCasKey();
		pathTable.GetString(outPath, stringOffset);
	}

	u8* CompactCasKeyTable::GetMemory()
	{
		return m_mem.memory;
	}

	u32 CompactCasKeyTable::GetSize()
	{
		SCOPED_READ_LOCK(m_lock, lock2)
		return u32(m_mem.writtenSize);
	}

	void CompactCasKeyTable::ReadMem(BinaryReader& reader, bool populateLookup)
	{
		if (!m_mem.memory)
			m_mem.Init(m_reserveSize);

		u64 writtenSize = m_mem.writtenSize;

		u64 left = reader.GetLeft();
		void* mem = m_mem.AllocateNoLock(left, 1, TC(""));
		reader.ReadBytes(mem, left);

		if (!populateLookup)
			return;

		BinaryReader reader2(m_mem.memory, writtenSize, m_mem.writtenSize);
		while (reader2.GetLeft())
		{
			u32 offset = u32(reader2.GetPosition());
			u64 stringOffset = reader2.Read7BitEncoded();
			CasKey casKey = reader2.ReadCasKey();
			m_offsets.try_emplace({casKey, u32(stringOffset)}, offset);
		}
	}

	void CompactCasKeyTable::Swap(CompactCasKeyTable& other)
	{
		m_offsets.swap(other.m_offsets);
		m_mem.Swap(other.m_mem);
		u64 rs = m_reserveSize;
		m_reserveSize = other.m_reserveSize;
		other.m_reserveSize = rs;
	}
}
