// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncRemote.h"

namespace unsync {

struct FCmdLoginOptions
{
	FRemoteDesc Remote;
	bool		bInteractive = false;
	bool		bPrint		 = false;
	bool		bDecode		 = false;
};

int32 CmdLogin(const FCmdLoginOptions& Options);

}  // namespace unsync
