// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatform.h"

namespace UE::DMX::GDTF
{
	/** The container for DMX Addresses */
	struct FDMXGDTFAddress
	{
		FDMXGDTFAddress() = default;

		FDMXGDTFAddress(int64 InAbsoluteAddress)
			: AbsoluteAddress(InAbsoluteAddress)
		{}

		FDMXGDTFAddress(int32 InUniverse, int32 InChannel)
		{
			AbsoluteAddress = InUniverse * UniverseSize + InChannel - 1;
		}

		int32 GetUniverse() const { return AbsoluteAddress / UniverseSize; }

		int32 GetChannel() const { return AbsoluteAddress % UniverseSize + 1; }

		int64 AbsoluteAddress = 512;

		static const uint16 UniverseSize = 512;
	};
}
