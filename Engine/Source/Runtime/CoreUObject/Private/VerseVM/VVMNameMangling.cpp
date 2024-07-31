// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMNameMangling.h"

namespace Verse
{

namespace Private
{
// Reserved name prefixes which will not be mangled.
#define VERSE_MANGLED_PREFIX "__verse_0x"
const char* const InternalNames[] =
	{
		// Avoid recursive mangling
		VERSE_MANGLED_PREFIX,

		// Generated names, no need to mangle
		"RetVal",
		"_RetVal",
		"$TEMP",
		"_Self",
};

bool ShouldMangleCasedName(const FString& Name)
{
	for (int32 i = 0; i < UE_ARRAY_COUNT(InternalNames); ++i)
	{
		if (Name.StartsWith(InternalNames[i]))
		{
			return false;
		}
	}
	return true;
}

} // namespace Private

bool MangleCasedName(const FString& Name, FString& MangledName)
{
	const bool bNameWasMangled = Private::ShouldMangleCasedName(Name);
	if (bNameWasMangled)
	{
		FString MangledString = TEXT(VERSE_MANGLED_PREFIX);
		auto AnsiString = StringCast<ANSICHAR>(*Name);
		const uint32 Crc = FCrc::StrCrc32(AnsiString.Get() ? AnsiString.Get() : "");
		MangledString.Append(BytesToHex(reinterpret_cast<const uint8*>(&Crc), sizeof(Crc)));
		MangledString.Append(TEXT("_"));
		MangledString.Append(Name);
		MangledName = MangledString;
	}
	else
	{
		MangledName = Name;
	}

	return bNameWasMangled;
}

bool MangleCasedNameCheck(const FString& Name, FName& MangledName)
{
	FString MangledNameStr;
	const bool bNameWasMangled = MangleCasedName(Name, MangledNameStr);
	MangledName = FName(MangledNameStr);
	return bNameWasMangled;
}

FString UnmangleCasedName(const FName MaybeMangledName, bool* bOutNameWasMangled)
{
	FString Result = MaybeMangledName.ToString();
	if (Result.StartsWith(TEXT("__verse_0x")))
	{
		// chop "__verse_0x" (10 char) + CRC (8 char) + "_" (1 char)
		Result = Result.RightChop(19);
		if (bOutNameWasMangled)
		{
			*bOutNameWasMangled = true;
		}
	}
	else if (bOutNameWasMangled)
	{
		*bOutNameWasMangled = false;
	}
	return Result;
}

#undef VERSE_MANGLED_PREFIX

} // namespace Verse
