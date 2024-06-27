// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Math/Box.h"
#include "UObject/WeakObjectPtr.h"

class UObject;

enum class ENavigationDirtyFlag : uint8
{
	None				= 0,
	Geometry			= (1 << 0),
	DynamicModifier		= (1 << 1),
	UseAgentHeight		= (1 << 2),
	NavigationBounds	= (1 << 3),

	All = Geometry | DynamicModifier, // all rebuild steps here without additional flags
};
ENUM_CLASS_FLAGS(ENavigationDirtyFlag);

struct FNavigationDirtyArea
{
	FBox Bounds = FBox(ForceInit);
	TWeakObjectPtr<UObject> OptionalSourceObject;
	ENavigationDirtyFlag Flags = ENavigationDirtyFlag::None;

	FNavigationDirtyArea() = default;
	ENGINE_API FNavigationDirtyArea(const FBox& InBounds, ENavigationDirtyFlag InFlags, UObject* const InOptionalSourceObject = nullptr);

	UE_DEPRECATED(5.5, "Use constructor taking ENavigationDirtyFlag instead.")
	ENGINE_API FNavigationDirtyArea(const FBox& InBounds, int32 InFlags, UObject* const InOptionalSourceObject = nullptr);

	bool HasFlag(const ENavigationDirtyFlag Flag) const
	{
		return (Flags & Flag) != ENavigationDirtyFlag::None;
	}

	bool operator==(const FNavigationDirtyArea& Other) const
	{ 
		return Flags == Other.Flags && OptionalSourceObject == Other.OptionalSourceObject && Bounds.Equals(Other.Bounds);
	}
	
	bool operator!=( const FNavigationDirtyArea& Other) const
	{
		return !(*this == Other);
	}
};
