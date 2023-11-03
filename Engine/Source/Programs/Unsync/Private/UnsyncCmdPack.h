// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncCore.h"

namespace unsync {

struct FCmdPackOptions
{
	FPath			  RootPath;
	FPath			  P4HavePath;  // optional
	bool			  bRunP4Have = false; // mutually exclusive with P4HavePath
	FPath			  StorePath;   // optional
	uint32			  BlockSize = uint32(64_KB);
	FAlgorithmOptions Algorithm;
};

int32 CmdPack(const FCmdPackOptions& Options);

}
