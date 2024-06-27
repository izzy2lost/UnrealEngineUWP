// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationDirtyArea.h"
#if !NO_LOGGING
#include "AI/NavigationSystemBase.h"
#endif

FNavigationDirtyArea::FNavigationDirtyArea(const FBox& InBounds, const ENavigationDirtyFlag InFlags, UObject* const InOptionalSourceObject)
	: Bounds(InBounds)
	, OptionalSourceObject(InOptionalSourceObject)
	, Flags(InFlags)
{
#if !NO_LOGGING
	if (!Bounds.IsValid || Bounds.ContainsNaN())
	{
		UE_LOG(LogNavigation, Warning, TEXT("Creation of FNavigationDirtyArea with invalid bounds%s. Bounds: %s, SourceObject: %s."),
			Bounds.ContainsNaN() ? TEXT(" (contains NaN)") : TEXT(""), *Bounds.ToString(), *GetFullNameSafe(OptionalSourceObject.Get()));
	}
#endif //!NO_LOGGING
}

// Deprecated
FNavigationDirtyArea::FNavigationDirtyArea(const FBox& InBounds, int32 InFlags, UObject* const InOptionalSourceObject /*= nullptr*/)
	: FNavigationDirtyArea(InBounds, static_cast<ENavigationDirtyFlag>(InFlags), InOptionalSourceObject)
{
}