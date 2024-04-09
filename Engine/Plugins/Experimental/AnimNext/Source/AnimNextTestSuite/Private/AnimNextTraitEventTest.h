// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/TraitUID.h"
#include "TraitCore/TraitEvent.h"

#include "AnimNextTraitEventTest.generated.h"

USTRUCT()
struct FTraitCoreTest_EventA : public FAnimNextTraitEvent
{
	GENERATED_BODY()
	DECLARE_ANIM_TRAIT_EVENT(FTraitCoreTest_EventA)

	bool bAlwaysForwardToBase = true;

	TArray<UE::AnimNext::FTraitUID> VisitedTraits;
};

USTRUCT()
struct FTraitCoreTest_EventB : public FAnimNextTraitEvent
{
	GENERATED_BODY()
	DECLARE_ANIM_TRAIT_EVENT(FTraitCoreTest_EventB)

	TArray<UE::AnimNext::FTraitUID> VisitedTraits;
};
