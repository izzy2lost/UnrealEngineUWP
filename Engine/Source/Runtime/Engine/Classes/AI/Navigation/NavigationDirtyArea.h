// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Math/Box.h"
#include "UObject/WeakObjectPtr.h"

class UObject;
namespace ENavigationDirtyFlag
{
	enum Type : uint8;
}

struct FNavigationDirtyArea
{
	FBox Bounds = FBox(ForceInit);
	int32 Flags = 0;
	TWeakObjectPtr<UObject> OptionalSourceObject;

	FNavigationDirtyArea();
	ENGINE_API FNavigationDirtyArea(const FBox& InBounds, int32 InFlags, UObject* const InOptionalSourceObject = nullptr);

	FORCEINLINE bool HasFlag(ENavigationDirtyFlag::Type Flag) const { return (Flags & Flag) != 0; }

	bool operator==(const FNavigationDirtyArea& Other) const
	{ 
		return Flags == Other.Flags && OptionalSourceObject == Other.OptionalSourceObject && Bounds.Equals(Other.Bounds);
	}
	
	bool operator!=( const FNavigationDirtyArea& Other) const
	{
		return !(*this == Other);
	}
};