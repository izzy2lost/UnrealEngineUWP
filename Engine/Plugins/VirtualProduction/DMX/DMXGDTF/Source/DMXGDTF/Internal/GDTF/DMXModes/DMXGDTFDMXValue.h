// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatform.h"

namespace UE::DMX::GDTF
{
	/**
	 * Special type to define DMX value where n is the byte count. The byte count can be individually specified without depending on the resolution of the DMX Channel.
	 * By default byte mirroring is used for the conversion. So 255/1 in a 16 bit channel will result in 65535.
	 * You can use the byte shifting operator to use byte shifting for the conversion. So 255/1s in a 16 bit channel will result in 65280.
	*/
	struct DMXGDTF_API FDMXGDTFDMXValue
	{
		FString Value;

		/** Returns the value as integer. Note this is a relatively slow operation as it parses the string. */
		bool ToInt(uint32& OutInteger) const;
	};
}
