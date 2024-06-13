// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFDMXValue.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFDMXValue::FDMXGDTFDMXValue(const FString& InValue)
	{
		Set(InValue);
	}

	FDMXGDTFDMXValue::FDMXGDTFDMXValue(const uint32 InValue)
	{
		Set(InValue);
	}

	FDMXGDTFDMXValue::FDMXGDTFDMXValue(const TOptional<uint32>& InOptionalValue)
	{
		if (InOptionalValue.IsSet())
		{
			Set(InOptionalValue.GetValue());
		}
	}

	uint32 FDMXGDTFDMXValue::AsIntChecked() const
	{
		check(IntegerValue.IsSet());

		return IntegerValue.GetValue();
	}

	bool FDMXGDTFDMXValue::IsSet() const
	{
		return IntegerValue.IsSet();
	}

	void FDMXGDTFDMXValue::Set(uint32 InValue)
	{
		IntegerValue = InValue;
		StringValue = *FString::Printf(TEXT("%u/1"), InValue);
	}

	void FDMXGDTFDMXValue::Set(const FString& InValue)
	{
		StringValue = InValue;
		if (StringValue.IsEmpty() || StringValue == TEXT("None"))
		{
			Reset();
			return;
		}

		TArray<FString> Substrings;
		StringValue.ParseIntoArray(Substrings, TEXT("/"));

		// As per GDTF specs
		if (Substrings.Num() != 2)
		{
			Reset();
			return;
		}

		uint32 DMXValue;
		if (!LexTryParseString(DMXValue, *Substrings[0]))
		{
			Reset();
			return;
		}

		const bool bUseByteShifting = Substrings[1].EndsWith(TEXT("s"));
		if (bUseByteShifting)
		{
			Substrings[1].RemoveFromEnd(TEXT("s"));

			uint8 ByteShiftValue;
			if (!LexTryParseString(ByteShiftValue, *Substrings[1]) ||
				ByteShiftValue > 4)
			{
				Reset();
				return;
			}
		
			IntegerValue = DMXValue << ByteShiftValue * 8;
			return;
		}
		else
		{
			uint8 ByteMirroringValue;
			if (!LexTryParseString(ByteMirroringValue, *Substrings[1]) ||
				ByteMirroringValue > 4)
			{
				Reset();
				return;
			}

			if (ByteMirroringValue == 1)
			{
				// No mirroring needed
				IntegerValue = DMXValue;
				return;
			}

			constexpr uint32 Masks[] = { 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 };
			if (ByteMirroringValue == 2)
			{
				IntegerValue = ((DMXValue & 0x000000FF) << 8) | ((DMXValue & 0x0000FF00) >> 8);
			}
			else if (ByteMirroringValue == 3)
			{
#if PLATFORM_LITTLE_ENDIAN
				IntegerValue = ((DMXValue & 0x0000FF00) << 16) | ((DMXValue & 0xFF000000) >> 16);
#else
				IntegerValue = ((DMXValue & 0x000000FF) << 16) | ((DMXValue & 0x00FF0000) >> 16);
#endif
			}
			else
			{
				IntegerValue = ((DMXValue & 0x000000FF) << 24) | ((DMXValue & 0x0000FF00) << 8) | ((DMXValue & 0x00FF0000) >> 8) | ((DMXValue & 0xFF000000) >> 24);
			}
		}
	}

	void FDMXGDTFDMXValue::Reset()
	{
		IntegerValue.Reset();
		StringValue = TEXT("None");
	}
}
