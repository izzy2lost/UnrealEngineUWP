// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SunAngleDaySequenceActor.h"

#include "SunPositionDaySequenceActor.generated.h"

/**
 * A simple procedural Day Sequence Actor that mimics an Earth day at a user-specified location and time.
 */
UCLASS(Blueprintable)
class DAYSEQUENCE_API ASunPositionDaySequenceActor
	: public AProceduralDaySequenceActor
{
	GENERATED_BODY()

public:
	ASunPositionDaySequenceActor(const FObjectInitializer& Init);

protected:
	virtual void Tick(float DeltaTime) override;

	void ApplySunAngle();
	
	virtual void BuildSequence(UProceduralDaySequenceBuilder* SequenceBuilder) override;
	virtual void SequencePlayerUpdated(float CurrentTime, float PreviousTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category= "Day Sequence", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDirectionalLightComponent> MoonComponent;
	
private:
	
	/** User settings */
	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	unsigned KeyCount = 24;
	
	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	FDateTime Time;

	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	double TimeZone;
	
	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	double Latitude;

	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	double Longitude;
	
	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	bool bIsDaylightSavings;

	
	/** Read only properties */
	UPROPERTY(VisibleAnywhere, Transient, Category=Sun, meta=(NoResetToDefault))
	double Elevation = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Transient, Category=Sun, meta=(NoResetToDefault))
	double Azimuth = 0.0f;

	
	/** Animated properties */
	UPROPERTY(Transient, Interp, Category=Sun)
	double SunPitch = 0.0f;

	UPROPERTY(Transient, Interp, Category=Sun)
	double SunYaw = 0.f;
};