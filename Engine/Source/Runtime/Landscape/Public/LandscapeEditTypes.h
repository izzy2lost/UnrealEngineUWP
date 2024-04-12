// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeEditTypes.generated.h"

UENUM()
enum class ELandscapeToolTargetType : uint8
{
	Heightmap = 0,
	Weightmap = 1,
	Visibility = 2,
	Invalid = 3 UMETA(Hidden), // only valid for LandscapeEdMode->CurrentToolTarget.TargetType
};

namespace UE::Landscape
{

enum class EOutdatedDataFlags : uint8
{
	None = 0,

	// Actual flags : 
	GrassMaps = (1 << 0),
	PhysicalMaterials = (1 << 1),
	NaniteMeshes = (1 << 2),
	PackageModified = (1 << 3),

	// Not real flags, only useful to loop through the actual flags : 
	LastPlusOne, 
	Last = LastPlusOne - 1,

	// Combined flags :
	All = (GrassMaps | PhysicalMaterials | NaniteMeshes | PackageModified)
};
ENUM_CLASS_FLAGS(EOutdatedDataFlags);

inline uint32 GetOutdatedDataFlagIndex(EOutdatedDataFlags InFlag)
{
	const uint64 InFlagAsUInt64 = static_cast<uint64>(InFlag);
	check((InFlagAsUInt64 > static_cast<uint64>(EOutdatedDataFlags::None)) && (InFlagAsUInt64 < static_cast<uint64>(EOutdatedDataFlags::LastPlusOne)) && (FMath::CountBits(InFlagAsUInt64) == 1u));
	return FMath::CountTrailingZeros(InFlagAsUInt64);
}

} // namespace UE::Landscape