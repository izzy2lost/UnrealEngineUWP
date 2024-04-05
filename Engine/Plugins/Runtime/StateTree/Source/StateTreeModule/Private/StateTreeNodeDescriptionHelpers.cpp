// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeNodeDescriptionHelpers.h"
#include "GameplayTagContainer.h"
#include "StateTreeNodeBase.h"

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "StateTree"

namespace UE::StateTree::DescHelpers
{

FText GetOperatorText(const EGenericAICheck Operator, EStateTreeNodeFormatting Formatting)
{
	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		switch (Operator)
		{
		case EGenericAICheck::Equal:
			return FText::FromString(TEXT("<s>==</>"));
		case EGenericAICheck::NotEqual:
			return FText::FromString(TEXT("<s>!=</>"));
		case EGenericAICheck::Less:
			return FText::FromString(TEXT("<s>&lt;</>"));
		case EGenericAICheck::LessOrEqual:
			return FText::FromString(TEXT("<s>&lt;=</>"));
		case EGenericAICheck::Greater:
			return FText::FromString(TEXT("<s>&gt;</>"));
		case EGenericAICheck::GreaterOrEqual:
			return FText::FromString(TEXT("<s>&gt;=</>"));
		default:
			break;
		}
	}
	else
	{
		switch (Operator)
		{
		case EGenericAICheck::Equal:
			return FText::FromString(TEXT("=="));
		case EGenericAICheck::NotEqual:
			return FText::FromString(TEXT("!="));
		case EGenericAICheck::Less:
			return FText::FromString(TEXT("&lt;"));
		case EGenericAICheck::LessOrEqual:
			return FText::FromString(TEXT("&lt;="));
		case EGenericAICheck::Greater:
			return FText::FromString(TEXT("&gt;"));
		case EGenericAICheck::GreaterOrEqual:
			return FText::FromString(TEXT("&gt;="));
		default:
			break;
		}
	}

	return FText::FromString(TEXT("??"));
}

FText GetInvertText(bool bInvert, EStateTreeNodeFormatting Formatting)
{
	FText InvertText = FText::GetEmpty();
	if (bInvert)
	{
		if (Formatting == EStateTreeNodeFormatting::RichText)
		{
			InvertText = LOCTEXT("InvertRich", "<s>Not</>  ");
		}
		else
		{
			InvertText = LOCTEXT("Invert", "Not  ");
		}
	}
	return InvertText;
}

FText GetBoolText(bool bValue, EStateTreeNodeFormatting Formatting)
{
	return bValue ? LOCTEXT("True", "True") : LOCTEXT("False", "False");
}

FText GetGameplayTagContainerAsText(const FGameplayTagContainer& TagContainer, const int ApproxMaxLength)
{
	if (TagContainer.IsEmpty())
	{
		return LOCTEXT("Empty", "Empty");
	}
		
	FString Combined;
	for (const FGameplayTag& Tag : TagContainer)
	{
		FString TagString = Tag.ToString();

		if (Combined.Len() > 0)
		{
			Combined += TEXT(", ");
		}
			
		if ((Combined.Len() + TagString.Len()) > ApproxMaxLength)
		{
			// Overflow
			if (Combined.Len() == 0)
			{
				Combined += TagString.Left(ApproxMaxLength);
			}
			Combined += TEXT("...");
			break;
		}

		Combined += TagString;
	}

	return FText::FromString(Combined);
}

FText GetGameplayTagQueryAsText(const FGameplayTagQuery& TagQuery, const int ApproxMaxLength)
{
	// Limit the presented query description.
	FText QueryValue = LOCTEXT("Empty", "Empty");
	constexpr int32 MaxDescLen = 120; 
	FString QueryDesc = TagQuery.GetDescription();
	if (!QueryDesc.IsEmpty())
	{
		if (QueryDesc.Len() > MaxDescLen)
		{
			QueryDesc = QueryDesc.Left(MaxDescLen);
			QueryDesc += TEXT("...");
		}
		QueryValue = FText::FromString(QueryDesc);
	}
	return QueryValue;
}

FText GetExactMatchText(bool bExactMatch, EStateTreeNodeFormatting Formatting)
{
	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return bExactMatch ? LOCTEXT("ExactlyRich", "<s>exactly</> ") : FText::GetEmpty();
	}
	return bExactMatch ? LOCTEXT("Exactly", "exactly ") : FText::GetEmpty();
}

} // UE::StateTree::Helpers

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR