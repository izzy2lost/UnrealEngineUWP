// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"
#include "UnsyncRemote.h"

#include <string>

namespace unsync {

struct FCmdQueryOptions
{
	std::string Query;
	FRemoteDesc Remote;
};

int32 CmdQuery(const FCmdQueryOptions& Options);

struct FMirrorInfo
{
	std::string Name;
	std::string Address;
	uint16		Port = UNSYNC_DEFAULT_PORT;
	double		Ping = 0;
};

TResult<FMirrorInfo> FindClosestMirror(const FRemoteDesc& Remote);

} // namespace unsync
