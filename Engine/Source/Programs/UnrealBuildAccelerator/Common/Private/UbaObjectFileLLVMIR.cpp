// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFileLLVMIR.h"

namespace uba
{
	// TODO: https://llvm.org/docs/BitCodeFormat.html


	bool ObjectFileLLVMIR::Parse(Logger& logger, const tchar* filename)
	{
		return logger.Error(TC("LLVM IR obj file format not supported (yet)"));
	}

	bool IsLLVMIRFile(const u8* data, u64 dataSize)
	{
		constexpr u8 wrapperMagic[] = { 'B', 'C', 0xc0, 0xde };
		constexpr u8 magic[] = { 'B', 'C', 0x04, 0xc4, 0xe4, 0xd4 };
		return dataSize >= 6 && (memcmp(data, wrapperMagic, sizeof(wrapperMagic)) == 0 || memcmp(data, magic, sizeof(magic)) == 0);
	}
}
