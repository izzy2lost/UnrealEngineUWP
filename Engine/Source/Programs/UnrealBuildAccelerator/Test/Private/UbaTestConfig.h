// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaConfig.h"

namespace uba
{
	bool TestConfig(Logger& logger, const StringBufferBase& rootDir)
	{
		static const char* configText =
			"[CacheClient]\r\n"
			"UseDirectoryPreparsing = true\r\n"
			"";

		Config config;
		if (!config.LoadFromText(logger, configText, strlen(configText)))
			return false;

		const ConfigTable& table = config.GetTable(TC("CacheClient"));
		bool test = false;
		if (!table.GetValueAsBool(test, TC("UseDirectoryPreparsing")))
			return false;
		if (test != true)
			return false;
		return true;
	}
}