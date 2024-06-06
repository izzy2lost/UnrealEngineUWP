// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStringBuffer.h"

namespace uba
{
	class Logger;

	class ConfigTable
	{
	public:
		bool GetValueAsString(StringBufferBase& out, const tchar* key) const;
		bool GetValueAsU32(u32& out, const tchar* key) const;
		bool GetValueAsBool(bool& out, const tchar* key) const;

		UnorderedMap<TString, TString> m_values;
	};

	class Config : public ConfigTable
	{
	public:
		bool LoadFromFile(Logger& logger, const tchar* configFile);
		bool LoadFromText(Logger& logger, const char* text, u64 textLen);
		bool IsLoaded() const;

		const ConfigTable& GetTable(const tchar* name) const;

		bool m_isLoaded = false;
		UnorderedMap<TString, ConfigTable> m_tables;
	};
}
