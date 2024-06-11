// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/SunAngleDaySequenceActor.h"

#include "Components/DirectionalLightComponent.h"
#include "ProceduralDaySequenceBuilder.h"

void ASunAngleDaySequenceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR
	ApplySunAngle();
#endif
}

void ASunAngleDaySequenceActor::ApplySunAngle()
{
	SunRootComponent->SetRelativeRotation(FRotator(0.0, SunYaw, SunRoll));
	SunComponent->SetRelativeRotation(FRotator(SunPitch + 90, 0.0, 0.0));
}

void ASunAngleDaySequenceActor::BuildSequence(UProceduralDaySequenceBuilder* SequenceBuilder)
{
	SequenceBuilder->SetActiveBoundObject(this);

	SequenceBuilder->AddScalarKey("SunPitch", 0.0, 0.0, RCIM_Linear);
	SequenceBuilder->AddScalarKey("SunPitch", 1.0, 360.0, RCIM_Linear);
}

void ASunAngleDaySequenceActor::SequencePlayerUpdated(float CurrentTime, float PreviousTime)
{
	ApplySunAngle();
}