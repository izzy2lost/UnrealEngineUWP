// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"
#include "Internationalization/Text.h"

enum EStateTreeNodeFormatting : uint8;
struct FGameplayTagContainer;
struct FGameplayTagQuery;

namespace UE::StateTree::DescHelpers
{
#if WITH_EDITOR

/** @return description for a EGenericAICheck. */
extern STATETREEMODULE_API FText GetOperatorText(const EGenericAICheck Operator, EStateTreeNodeFormatting Formatting);

/** @return description for condition inversion (returns "Not" plus a space). */
extern STATETREEMODULE_API FText GetInvertText(bool bInvert, EStateTreeNodeFormatting Formatting);

/** @return description of a boolean value. */
extern STATETREEMODULE_API FText GetBoolText(bool bValue, EStateTreeNodeFormatting Formatting);

/** @return description of being within a value range. */
extern STATETREEMODULE_API FText GetWithinValueRangeText(float Min, float Max, EStateTreeNodeFormatting Formatting);

/** @return description for a Gameplay Tag Container. If the length of container description is longer than ApproxMaxLength, the it truncated and ... as added to the end. */
extern STATETREEMODULE_API FText GetGameplayTagContainerAsText(const FGameplayTagContainer& TagContainer, const int ApproxMaxLength = 60);

/** @return description for a Gameplay Tag Query. If the query description is longer than ApproxMaxLength, the it truncated and ... as added to the end. */
extern STATETREEMODULE_API FText GetGameplayTagQueryAsText(const FGameplayTagQuery& TagQuery, const int ApproxMaxLength = 120);

/** @return description for exact match, used for Gameplay Tag matching functions (returns "Exactly" plus space). */
extern STATETREEMODULE_API FText GetExactMatchText(bool bExactMatch, EStateTreeNodeFormatting Formatting);

#endif // WITH_EDITOR 
} // UE::StateTree::Helpers