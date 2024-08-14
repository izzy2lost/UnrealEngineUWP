// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationDirtyArea.h"
#include "AI/Navigation/NavigationElement.h"
#if !NO_LOGGING
#include "AI/NavigationSystemBase.h"
#endif

FNavigationDirtyArea::FNavigationDirtyArea(const FBox& InBounds, const ENavigationDirtyFlag InFlags, const TSharedPtr<const FNavigationElement>& InOptionalSourceElement)
	: Bounds(InBounds)
	, OptionalSourceElement(InOptionalSourceElement)
	, Flags(InFlags)
{
#if !NO_LOGGING
	if (!Bounds.IsValid || Bounds.ContainsNaN())
	{
		const FNavigationElement* Element = OptionalSourceElement.Get();
		UE_LOG(LogNavigation, Warning, TEXT("Creation of FNavigationDirtyArea with invalid bounds%s. Bounds: %s, SourceElement: %s."),
			Bounds.ContainsNaN() ? TEXT(" (contains NaN)") : TEXT(""), *Bounds.ToString(), *GetFullNameSafe(Element));
	}
#endif //!NO_LOGGING
}

FString FNavigationDirtyArea::GetSourceDescription() const
{
	const FNavigationElement* SourceElement = OptionalSourceElement.Get();
	return SourceElement ? SourceElement->GetFullName() : TEXT("");
}

// Deprecated
FNavigationDirtyArea::FNavigationDirtyArea(const FBox& InBounds, const int32 InFlags, UObject* const InOptionalSourceObject)
	: FNavigationDirtyArea(InBounds, static_cast<ENavigationDirtyFlag>(InFlags), /*InOptionalSourceElement*/nullptr)
{
}
