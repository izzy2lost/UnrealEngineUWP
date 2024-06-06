// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaConfig.h"
#include "UbaFileAccessor.h"

namespace uba
{
	static ConfigTable& EmptyConfigTable = *new ConfigTable();

	bool ConfigTable::GetValueAsString(StringBufferBase& out, const tchar* key) const
	{
		return false;
	}

	bool ConfigTable::GetValueAsU32(u32& out, const tchar* key) const
	{
		return false;
	}

	bool ConfigTable::GetValueAsBool(bool& out, const tchar* key) const
	{
		auto findIt = m_values.find(key);
		if (findIt == m_values.end())
			return false;
		const tchar* value = findIt->second.c_str();
		if (Equals(value, TC("true")) || Equals(value, TC("1")))
		{
			out = true;
			return true;
		}
		if (Equals(value, TC("false")) || Equals(value, TC("0")))
		{
			out = false;
			return true;
		}

		return false;
	}

	bool Config::LoadFromFile(Logger& logger, const tchar* configFile)
	{
		m_isLoaded = true;

		FileAccessor fa(logger, configFile);
		if (!fa.OpenMemoryRead(0, false))
			return false;
		return LoadFromText(logger, (const char*)fa.GetData(), fa.GetSize());
	}

	bool Config::LoadFromText(Logger& logger, const char* text, u64 textLen)
	{
		m_isLoaded = true;

		const char* i = text;
		const char* e = i + textLen;

		auto consumeEmpty = [&]() -> char
			{
				while (i != e)
				{
					if (*i != ' ' && *i != '\t'&& *i != '\r')
						return *i;
					++i;
				}
				return 0;
			};

		auto consumeIdentifier = [&](StringBufferBase& out) -> char
			{
				while (i != e)
				{
					if (!((*i >= 'a' && *i <= 'z') || (*i >= 'A' && *i <= 'Z')))
						return *i;
					out.Append(*i);
					++i;
				}
				return 0;
			};

		auto consumeLine = [&](StringBufferBase& out) -> char
			{
				while (i != e)
				{
					if (*i == '\n')
						return *i;
					if (*i != '\r')
						out.Append(*i);
					++i;
				}
				return 0;
			};

		ConfigTable globalTable;
		ConfigTable* activeTable = &globalTable;
		while (true)
		{
			char token = consumeEmpty();
			if (token == 0)
				break;
			if (token == '\n')
			{
				++i;
			}
			else if (token == '[')
			{
				++i;
				StringBuffer<128> tableName;
				token = consumeIdentifier(tableName);
				if (token != ']')
					return logger.Error(TC("No end token after group name %s"), tableName.data);
				++i;
				token = consumeEmpty();
				if (token == 0)
					break;
				if (token != '\n')
					return logger.Error(TC("Unexpected token %c after group %s"), tableName.data);
				++i;
				activeTable = &m_tables.try_emplace(tableName.data).first->second;
			}
			else
			{
				StringBuffer<128> key;
				consumeIdentifier(key);
				token = consumeEmpty();
				if (token != '=')
					return logger.Error(TC("Unexpected equals sign after key name %s"), key);
				++i;
				token = consumeEmpty();
				if (token == '\"')
				{
					return logger.Error(TC("Strings not supported yet %s"), key);
				}
				else
				{
					StringBuffer<128> value;
					token = consumeIdentifier(value);
					activeTable->m_values[key.data] = value.data;
					if (token == 0)
						break;
					++i;
				}
			}
		}

		return true;
	}

	bool Config::IsLoaded() const
	{
		return m_isLoaded;
	}

	const ConfigTable& Config::GetTable(const tchar* name) const
	{
		auto findIt = m_tables.find(name);
		if (findIt == m_tables.end())
			return EmptyConfigTable;
		return findIt->second;
	}
}