// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatform.h"
#include "Misc/Optional.h"

namespace UE::DMX::GDTF
{
	/**
	 * Special type to define DMX value where n is the byte count. The byte count can be individually specified without depending on the resolution of the DMX Channel.
	 * By default byte mirroring is used for the conversion. So 255/1 in a 16 bit channel will result in 65535.
	 * You can use the byte shifting operator to use byte shifting for the conversion. So 255/1s in a 16 bit channel will result in 65280.
	*/
	struct DMXGDTF_API FDMXGDTFDMXValue
	{
		FDMXGDTFDMXValue() = default;
		FDMXGDTFDMXValue(const FString& InValue);
		FDMXGDTFDMXValue(const uint32 InValue);
		FDMXGDTFDMXValue(const TOptional<uint32>& InValue);

		bool operator==(const FDMXGDTFDMXValue& Other) const { return IntegerValue == Other.IntegerValue; }
		bool operator!=(const FDMXGDTFDMXValue& Other) const { return IntegerValue != Other.IntegerValue; }

		/** Returns the value as an optional integer. If the optional is not set, it equals to "None". */
		TOptional<uint32> AsInt() const { return IntegerValue; }

		/** Returns the value as an integer. Assumes the int is set (checked). */
		uint32 AsIntChecked() const;

		/** Returns the value as string. */
		FString AsString() const { return StringValue; }

		/** Returns true if the value is set. Otherwise it uses special value "None". */
		bool IsSet() const;

		/** Sets the DMX Value by int. */
		void Set(uint32 InValue);

		/** Sets the value by string. Special value "None" resets the optional int. */
		void Set(const FString& InValue);

		/** Resets to None. */
		void Reset();

	private:
		FString StringValue = TEXT("None");
		TOptional<uint32> IntegerValue;
	};
}
