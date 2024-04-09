// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/TraitUID.h"
#include "TraitCore/TraitEvent.h"

#include "AnimNextAnimGraphTraitEventTest.generated.h"

USTRUCT()
struct FTraitAnimGraphTest_EventA : public FAnimNextTraitEvent
{
	GENERATED_BODY()
	DECLARE_ANIM_TRAIT_EVENT(FTraitAnimGraphTest_EventA)

	bool bTestFlag = false;

	TArray<UE::AnimNext::FTraitUID> VisitedTraits;
};

USTRUCT()
struct FTraitAnimGraphTest_EventB : public FAnimNextTraitEvent
{
	GENERATED_BODY()
	DECLARE_ANIM_TRAIT_EVENT(FTraitAnimGraphTest_EventB)

	bool bTestFlag0 = false;
	bool bTestFlag1 = false;

	TArray<UE::AnimNext::FTraitUID> VisitedTraits;

	FAnimNextTraitEventPtr ChildEvent;
};
