// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Version.h"

////////////////////////////////////////////////////////////////////////////////
struct FInstanceInfo
{
public:
	static const uint32	CurrentVersion =
#if TS_USING(TS_BUILD_DEBUG)
		0x8000'0000 |
#endif
		((TS_VERSION_PROTOCOL & 0xffff) << 16) | (TS_VERSION_MINOR & 0xffff);

	void				Set();
	void				WaitForReady() const;
	bool				IsOlder() const;
	std::atomic<uint32> Published;
	uint32				Version;
	uint32				Pid;
};


