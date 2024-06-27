// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Templates/SubclassOf.h"
#include "Tags/ScriptableToolGroupTag.h"

#include "ScriptableToolGroupSet.generated.h"

USTRUCT()
struct FScriptableToolGroupSet
{
	GENERATED_BODY()

	/**
	 * Note: This type needs to be specified explicitly for Groups because we can't use a typedef for a UPROPERTY.
	 */
	typedef TSet<TSubclassOf<UScriptableToolGroupTag>> FGroupSet;

	UPROPERTY(EditAnywhere, Category = "Groups")
	TSet<TSubclassOf<UScriptableToolGroupTag>> Groups;

public:

	bool Matches(const FScriptableToolGroupSet& OtherSet) const
	{
		return !Groups.Intersect(OtherSet.Groups).IsEmpty();
	}
};