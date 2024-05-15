// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFile.h"
#include "UbaFileAccessor.h"
#include "UbaObjectFileCoff.h"
#include "UbaObjectFileElf.h"

namespace uba
{
	ObjectFile* ObjectFile::CreateAndParse(Logger& logger, const tchar* filename)
	{
		auto file = new FileAccessor(logger, filename);
		auto fileGuard = MakeGuard([&]() { delete file; });

		if (!file->OpenMemoryRead())
			return nullptr;

		u8* data = file->GetData();
		u64 dataSize = file->GetSize();

		if (dataSize < 4)
			return nullptr;

		ObjectFile* objectFile = nullptr;

		constexpr u8 elfMagic[] = { 0x7f, 'E', 'L', 'F' };
		if (memcmp(data, elfMagic, sizeof(elfMagic)) == 0)
			objectFile = new ObjectFileElf();
		else
			objectFile = new ObjectFileCoff();

		fileGuard.Cancel();
		objectFile->m_file = file;
		objectFile->m_data = data;
		objectFile->m_dataSize = dataSize;

		if (objectFile->Parse(logger, filename))
			return objectFile;

		delete objectFile;
		return nullptr;
	}


	ObjectFile::~ObjectFile()
	{
		delete m_file;
	}

	const tchar* ObjectFile::GetFileName() const
	{
		return m_file->GetFileName();
	}

	const UnorderedSymbols& ObjectFile::GetImports() const
	{
		return m_imports;
	}

	const UnorderedSymbols& ObjectFile::GetExports() const
	{
		return m_exports;
	}

	const UnorderedSymbols& ObjectFile::GetPotentialDuplicates() const
	{
		return m_potentialDuplicates;
	}
}
