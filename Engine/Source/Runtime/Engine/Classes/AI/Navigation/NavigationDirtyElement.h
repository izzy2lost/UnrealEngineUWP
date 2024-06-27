// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"
#include "UObject/WeakObjectPtr.h"

enum class ENavigationDirtyFlag : uint8;
class INavRelevantInterface;

struct FNavigationDirtyElement
{
	/**
	 * If not empty and the associated navigation relevant object controls the dirty areas explicitly (i.e. ShouldSkipDirtyAreaOnAddOrRemove returns true),
	 * the list will be used to indicate the areas that need rebuilding.
	 * Otherwise, the default behavior, NavRelevant object's bounds will be used.
	 */
	TArray<FBox> ExplicitAreasToDirty;

	/** object owning this element */
	FWeakObjectPtr Owner;

	/** cached interface pointer */
	INavRelevantInterface* NavInterface = nullptr;

	/** bounds of already existing entry for this actor */
	FBox PrevBounds = FBox(ForceInit);

	/** override for update flags */
	ENavigationDirtyFlag FlagsOverride;

	/** flags of already existing entry for this actor */
	ENavigationDirtyFlag PrevFlags;

	/** prev flags & bounds data are set */
	uint8 bHasPrevData : 1 = false;

	/** request was invalidated while queued, use prev values to dirty area */
	uint8 bInvalidRequest : 1 = false;

	/** requested during visibility change of the owning level (loading/unloading) */
	uint8 bIsFromVisibilityChange : 1 = false;

	/** part of the base navmesh */
	uint8 bIsInBaseNavmesh : 1 = false;

	ENGINE_API FNavigationDirtyElement();
	ENGINE_API explicit FNavigationDirtyElement(UObject* InOwner);
	ENGINE_API FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, ENavigationDirtyFlag InFlagsOverride, const bool bUseWorldPartitionedDynamicMode = false);
	ENGINE_API FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, const bool bUseWorldPartitionedDynamicMode = false);

	UE_DEPRECATED(5.5, "Use the version taking ENavigationDirtyFlag instead.")
	ENGINE_API FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, int32 InFlagsOverride = 0, const bool bUseWorldPartitionedDynamicMode = false);

	bool operator==(const FNavigationDirtyElement& Other) const 
	{ 
		return Owner == Other.Owner; 
	}

	bool operator==(const UObject*& OtherOwner) const 
	{ 
		return (Owner == OtherOwner);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FNavigationDirtyElement& Info)
	{
		return GetTypeHash(Info.Owner);
	}
};
