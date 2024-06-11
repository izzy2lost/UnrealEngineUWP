// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralDaySequenceActor.h"

#include "SunAngleDaySequenceActor.generated.h"

/**
 * A simple procedural Day Sequence Actor that positions the sun according to SunPitch, SunYaw, and SunRoll.
 * A sequence is generated on the fly that animates SunPitch linearly over the length of the day.
 * SunYaw and SunRoll can be manually configured to set the sun's elevation and the direction it moves across the sky.
 */
UCLASS(Blueprintable)
class DAYSEQUENCE_API ASunAngleDaySequenceActor
	: public AProceduralDaySequenceActor
{
	GENERATED_BODY()

protected:
	virtual void Tick(float DeltaTime) override;
	
	void ApplySunAngle();
	
	virtual void BuildSequence(UProceduralDaySequenceBuilder* SequenceBuilder) override;
	virtual void SequencePlayerUpdated(float CurrentTime, float PreviousTime) override;

protected:
	// Determines how far the sun is along its path.
	UPROPERTY(VisibleAnywhere, Interp, Category=Sun, meta=(NoResetToDefault))
	double SunPitch = 0.0f;

	// Determines the path the sun takes through the sky (for example, determines if the sun moves east to west).
	UPROPERTY(EditAnywhere, Interp, Category=Sun, meta=(UIMin = "-180", UIMax = "180", ClampMin="-180", ClampMax="180", SliderExponent=1))
	double SunYaw = 0.f;

	// Determines how high above the horizon the sun will be at its peak.
	UPROPERTY(EditAnywhere, Interp, Category=Sun, meta=(UIMin = "-90", UIMax = "90", ClampMin="-180", ClampMax="180", SliderExponent=1))
	double SunRoll = 0.f;
};
