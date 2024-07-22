// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorWiggle.h"

#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "PropertyAnimatorShared.h"

UPropertyAnimatorWiggle::UPropertyAnimatorWiggle()
{
	static int32 SeedIncrement = 0;

	SetAnimatorDisplayName(DefaultControllerName);

	bRandomTimeOffset = true;
	Seed = SeedIncrement++;
}

bool UPropertyAnimatorWiggle::EvaluateProperty(const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorCoreContext* InContext, FInstancedPropertyBag& InParameters, FInstancedPropertyBag& OutEvaluationResult) const
{
	const double TimeElapsed = InParameters.GetValueDouble(TimeElapsedParameterName).GetValue();
	const double Frequency = InParameters.GetValueDouble(FrequencyParameterName).GetValue();

	// Apply random wave based on time and frequency
	const double WaveResult = UE::PropertyAnimator::Wave::Perlin(TimeElapsed, 1.f, Frequency, 0.f);

	// Remap from [-1, 1] to user amplitude from [Min, Max]
	const float NormalizedValue = FMath::GetMappedRangeValueClamped(FVector2D(-1, 1), FVector2D(0, 1), WaveResult);

	InParameters.AddProperty(AlphaParameterName, EPropertyBagPropertyType::Float);
	InParameters.SetValueFloat(AlphaParameterName, NormalizedValue);

	return InContext->EvaluateProperty(InPropertyData, InParameters, OutEvaluationResult);
}
