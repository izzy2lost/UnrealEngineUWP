// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFDMXValue.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFDMXValue::FDMXGDTFDMXValue(const TCHAR* InValue)
		: Value(InValue)
	{}

	bool FDMXGDTFDMXValue::ToInt(uint32& OutInteger) const
	{
		if (Value.IsEmpty() || Value == TEXT("None"))
		{
			return false;
		}

		TArray<FString> Substrings;
		Value.ParseIntoArray(Substrings, TEXT("/"));

		// As per GDTF specs
		if (Substrings.Num() == 2)
		{
			uint32 DMXValue;
			if (!LexTryParseString(DMXValue, *Substrings[0]))
			{
				return false;
			}

			if (Substrings[1].EndsWith(TEXT("s")))
			{
				Substrings[1].RemoveFromEnd(TEXT("s"));

				uint8 ByteShiftValue;
				if (LexTryParseString(ByteShiftValue, *Substrings[1]))
				{
					if (ByteShiftValue == 1)
					{
						// No shifting needed
						OutInteger = DMXValue;
						return true;
					}
					else if (ByteShiftValue > 4)
					{
						return false;
					}

					OutInteger = DMXValue << (ByteShiftValue - 1) * 8;
					return true;
				}
			}
			else
			{
				uint8 ByteMirroringValue;
				if (LexTryParseString(ByteMirroringValue, *Substrings[1]))
				{
					if (ByteMirroringValue == 1)
					{
						// No mirroring needed
						OutInteger = DMXValue;
						return true;
					}
					else if (ByteMirroringValue > 4)
					{
						return false;
					}
					else
					{
						constexpr uint32 Masks[] = { 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 };

						if (ByteMirroringValue == 2)
						{
							OutInteger = ((DMXValue & 0x000000FF) << 8) | ((DMXValue & 0x0000FF00) >> 8);
						}
						else if (ByteMirroringValue == 3)
						{
#if PLATFORM_LITTLE_ENDIAN
							OutInteger = ((DMXValue & 0x0000FF00) << 16) | ((DMXValue & 0xFF000000) >> 16);
#else
							OutInteger = ((DMXValue & 0x000000FF) << 16) | ((DMXValue & 0x00FF0000) >> 16);
#endif
						}
						else
						{
							OutInteger = ((DMXValue & 0x000000FF) << 24) | ((DMXValue & 0x0000FF00) << 8) | ((DMXValue & 0x00FF0000) >> 8) | ((DMXValue & 0xFF000000) >> 24);
						}
					}

					return true;
				}
			}
		}

		return false;
	}
}
