// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaObjectFileElf.h"
#include "UbaFileAccessor.h"

namespace uba
{

	bool ObjectFileElf::Parse(Logger& logger, const tchar* filename)
	{
		return true;
	}

	bool ObjectFileElf::ComputeLoopbacksAndDuplicates(UnorderedSymbols& allSharedExports, UnorderedSymbols& duplicates)
	{
		return true;
	}

	bool ObjectFileElf::CreateStripped(Logger& logger, const tchar* newFilename, const UnorderedSymbols& allNeededImports)
	{
		FileAccessor newFile(logger, newFilename);
		if (!newFile.CreateMemoryWrite(false, DefaultAttributes(), m_dataSize))
			return false;

		u8* newData = newFile.GetData();
		memcpy(newData, m_data, m_dataSize);

		return newFile.Close();
	}
}
