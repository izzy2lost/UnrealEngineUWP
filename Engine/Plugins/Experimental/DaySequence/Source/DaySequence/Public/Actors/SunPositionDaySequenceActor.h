// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimpleDaySequenceActor.h"

#include "SunPositionDaySequenceActor.generated.h"

/**
 * A Day Sequence Actor that represents a physically accurate 24 hour day cycle.
 */
UCLASS(Blueprintable)
class DAYSEQUENCE_API ASunPositionDaySequenceActor
	: public ASimpleDaySequenceActor
{
	GENERATED_BODY()

public:
	ASunPositionDaySequenceActor(const FObjectInitializer& Init);

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category= "Day Sequence", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDirectionalLightComponent> MoonComponent;
};