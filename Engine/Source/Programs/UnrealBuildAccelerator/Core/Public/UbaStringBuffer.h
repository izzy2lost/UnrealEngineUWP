// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaMemory.h"

namespace uba
{
	bool StartsWith(const tchar* data, const tchar* str, bool ignoreCase = true);
	bool EndsWith(const tchar* str, u64 strLen, const tchar* value, bool ignoreCase = true);
	bool Contains(const tchar* str, const tchar* sub, bool ignoreCase = true, const tchar** pos = nullptr);
	bool Equals(const tchar* str1, const tchar* str2, bool ignoreCase = true);
	bool Equals(const tchar* str1, const tchar* str2, u64 count, bool ignoreCase = true);
	void Replace(tchar* str, tchar from, tchar to);
	inline void ToLower(tchar* str) { while (tchar c = *str) { if (c >= 'A' && c <= 'Z') *str = c - 'A' + 'a'; ++str; } }
	inline tchar ToLower(tchar c) { return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;}
	inline tchar ToUpper(tchar c) { return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;}

	class StringBufferBase
	{
	public:
		StringBufferBase& Append(const StringBufferBase& str) { return Append(str.data, str.count); }
		StringBufferBase& Append(const TString& str) { return Append(str.data(), str.size()); }
		StringBufferBase& Append(const tchar* str);
		StringBufferBase& Append(const tchar c) { return Append(&c, 1); }
		StringBufferBase& Append(const tchar* str, u64 charCount);
		StringBufferBase& Appendf(const tchar* format, ...);
		StringBufferBase& AppendDir(const StringBufferBase& str);
		StringBufferBase& AppendDir(const tchar* dir);
		StringBufferBase& AppendFileName(const tchar* str);
		StringBufferBase& AppendHex(u64 v);
		StringBufferBase& AppendValue(u64 v);
		StringBufferBase& Append(const tchar* format, va_list& args);
		StringBufferBase& Resize(u64 newSize);
		StringBufferBase& Clear();

		tchar operator[](u64 i) const { return data[i]; }
		tchar& operator[](u64 i) { return data[i]; }

		bool IsEmpty() const { return count == 0; }
		bool StartsWith(const tchar* str, bool ignoreCase = true) const { return uba::StartsWith(data, str, ignoreCase); }
		bool EndsWith(const tchar* value, bool ignoreCase = true) const { return uba::EndsWith(data, count, value, ignoreCase); }
		bool Contains(tchar c) const;
		bool Contains(const tchar* str, bool ignoreCase = true) const { return uba::Contains(data, str, ignoreCase); }
		bool Equals(const tchar* str, bool ignoreCase = true) const { return uba::Equals(data, str, ignoreCase); }
		const tchar* First(tchar c, u64 offset = 0) const;
		const tchar* Last(tchar c, u64 offset = 0) const;
		inline StringBufferBase& Replace(tchar from, tchar to) { uba::Replace(data, from, to);  return *this; }

		StringBufferBase& EnsureEndsWithSlash();

		StringBufferBase& MakeLower()
		{
			for (tchar* it = data; *it; ++it)
				*it = ToLower(*it);
			return *this;
		}

		bool Parse(u64& out)
		{
			if (!count)
				return false;

			#if PLATFORM_WINDOWS
			out = wcstoull(data, nullptr, 10);
			#else
			out = strtoull(data, nullptr, 10);
			#endif
			return out != 0 || Equals(TC("0"));
		}

		bool Parse(u32& out)
		{
			if (!count)
				return false;
			#if PLATFORM_WINDOWS
			out = wcstoul(data, nullptr, 10);
			#else
			out = strtoul(data, nullptr, 10);
			#endif
			return out != 0 || Equals(TC("0"));
		}

		bool Parse(u16& out)
		{
			u32 temp;
			if (!Parse(temp))
				return false;
			if (temp > 65535)
				return false;
			out = u16(temp);
			return true;
		}

		bool Parse(float& out)
		{
			if (!count)
				return false;
			#if PLATFORM_WINDOWS
			out = wcstof(data, nullptr);
			#else
			out = strtof(data, nullptr);
			#endif
			return out != 0 || Equals(TC("0"));
		}

		u32 count;
		u32 capacity;
		tchar data[1];

	protected:
		StringBufferBase(u32 c) : capacity(c) { count = 0; *data = 0; }
	};


	template<u32 Capacity = 512>
	class StringBuffer : public StringBufferBase
	{
	public:
		StringBuffer() : StringBufferBase(Capacity) { *buf = 0; }
		explicit StringBuffer(const TString& str) : StringBufferBase(Capacity) { *buf = 0; Append(str); }
		explicit StringBuffer(const tchar* str) : StringBufferBase(Capacity) { *buf = 0; if (str) Append(str); }
		template<u32 Capacity2>
		explicit StringBuffer(const StringBuffer<Capacity2>& str) : StringBufferBase(Capacity) { *buf = 0; Append(str); }
	private:
		tchar buf[Capacity];
	};

	struct LastErrorToText : StringBuffer<256>
	{
		LastErrorToText();
		LastErrorToText(u32 lastError);
		operator const tchar* () const { return data; };
	};
}