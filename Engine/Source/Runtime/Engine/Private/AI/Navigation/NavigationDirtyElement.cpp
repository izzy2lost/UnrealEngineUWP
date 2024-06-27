// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationDirtyElement.h"
#include "AI/Navigation/NavigationDirtyArea.h"
#include "AI/Navigation/NavigationTypes.h"

FNavigationDirtyElement::FNavigationDirtyElement(
	UObject* InOwner,
	INavRelevantInterface* InNavInterface,
	const ENavigationDirtyFlag InFlagsOverride,
	const bool bUseWorldPartitionedDynamicMode /*= false*/)
	: Owner(InOwner)
	, NavInterface(InNavInterface)
	, FlagsOverride(InFlagsOverride)
	, PrevFlags(ENavigationDirtyFlag::None)
{
	if (bUseWorldPartitionedDynamicMode)
	{
		bIsFromVisibilityChange = FNavigationSystem::IsLevelVisibilityChanging(InOwner);
		bIsInBaseNavmesh = FNavigationSystem::IsInBaseNavmesh(InOwner);
	}
	else
	{
		bIsFromVisibilityChange = false;
		bIsInBaseNavmesh = false;
	}
}

FNavigationDirtyElement::FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, const bool bUseWorldPartitionedDynamicMode /*= false*/)
	: FNavigationDirtyElement(InOwner, InNavInterface, ENavigationDirtyFlag::None, bUseWorldPartitionedDynamicMode)
{
}

FNavigationDirtyElement::FNavigationDirtyElement(UObject* InOwner)
	: FNavigationDirtyElement(InOwner, /*InNavInterface*/nullptr, /*bUseWorldPartitionedDynamicMode*/false)
{
}

FNavigationDirtyElement::FNavigationDirtyElement()
	: FNavigationDirtyElement(/*InOwner*/nullptr, /*InNavInterface*/nullptr, /*bUseWorldPartitionedDynamicMode*/false)
{
}

// Deprecated
FNavigationDirtyElement::FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, int32 InFlagsOverride /*= 0*/, const bool bUseWorldPartitionedDynamicMode /*= false*/)
	: FNavigationDirtyElement(InOwner, InNavInterface, static_cast<ENavigationDirtyFlag>(InFlagsOverride), bUseWorldPartitionedDynamicMode)
{
}
