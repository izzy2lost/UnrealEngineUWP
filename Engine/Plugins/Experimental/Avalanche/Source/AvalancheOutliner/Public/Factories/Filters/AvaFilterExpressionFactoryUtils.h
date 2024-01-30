// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Item/AvaOutlinerActor.h"
#include "Misc/TextFilterUtils.h"

namespace UE::AvalancheFactory::Private
{
	bool inline FilterActorComponentsByType(const FAvaOutlinerActor* InAvaOutlinerActor, FName InValueToCheck, ETextFilterComparisonOperation InComparisonOperation,ETextFilterTextComparisonMode InComparisonMode)
	{
		TArray<UActorComponent*> Components;
		InAvaOutlinerActor->GetActor()->GetComponents<UActorComponent>(Components);
		for (const UActorComponent* Component : Components)
		{
			FString ComponentName = Component->GetClass()->GetFName().ToString();
			ComponentName.RemoveSpacesInline();
			ComponentName.ToUpperInline();
			const bool bIsComponentMatch = TextFilterUtils::TestBasicStringExpression(ComponentName, InValueToCheck, InComparisonMode);
			if (InComparisonOperation == ETextFilterComparisonOperation::Equal ? bIsComponentMatch : !bIsComponentMatch)
			{
				return true;
			}
		}
		return false;
	}

	bool inline FilterActorComponentsByName(const FAvaOutlinerActor* InAvaOutlinerActor, FName InValueToCheck, ETextFilterComparisonOperation InComparisonOperation,ETextFilterTextComparisonMode InComparisonMode)
	{
		TArray<UActorComponent*> Components;
		InAvaOutlinerActor->GetActor()->GetComponents<UActorComponent>(Components);
		for (const UActorComponent* Component : Components)
		{
			FString ComponentName = Component->GetName();
			ComponentName.RemoveSpacesInline();
			ComponentName.ToUpperInline();
			const bool bIsComponentMatch = TextFilterUtils::TestBasicStringExpression(ComponentName, InValueToCheck, InComparisonMode);
			if (InComparisonOperation == ETextFilterComparisonOperation::Equal ? bIsComponentMatch : !bIsComponentMatch)
			{
				return true;
			}
		}
		return false;
	}
}
