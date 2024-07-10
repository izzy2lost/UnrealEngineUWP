// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStringBuffer.h"

namespace uba
{
	class Config;
	class Logger;


	class ConfigTable
	{
	public:
		bool GetValueAsString(const tchar*& out, const tchar* key) const;
		bool GetValueAsU32(u32& out, const tchar* key) const;
		bool GetValueAsInt(int& out, const tchar* key) const;
		bool GetValueAsBool(bool& out, const tchar* key) const;

		const ConfigTable* GetTable(const tchar* name) const;

		void AddValue(const tchar* key, int value);
		void AddValue(const tchar* key, bool value);

	private:
		ConfigTable* m_parent = nullptr;
		Map<TString, TString> m_values;
		UnorderedMap<TString, ConfigTable> m_tables;
		friend Config;
	};


	class Config : public ConfigTable
	{
	public:
		bool LoadFromFile(Logger& logger, const tchar* configFile);
		bool LoadFromText(Logger& logger, const char* text, u64 textLen);
		bool IsLoaded() const;

		bool SaveToFile(Logger& logger, const tchar* configFile);

		bool m_isLoaded = false;
	};
}
