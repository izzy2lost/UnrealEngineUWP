// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorCurve.h"

#include "Curves/PropertyAnimatorEaseCurve.h"
#include "Curves/PropertyAnimatorWaveCurve.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "UObject/ConstructorHelpers.h"

UPropertyAnimatorCurve::UPropertyAnimatorCurve()
{
	SetAnimatorDisplayName(DefaultAnimatorName);

	static const ConstructorHelpers::FObjectFinder<UPropertyAnimatorWaveCurve> SineCurve(TEXT("/Script/PropertyAnimator.PropertyAnimatorWaveCurve'/PropertyAnimator/Waves/Sine.Sine'"));

	if (SineCurve.Succeeded())
	{
		WaveCurve = SineCurve.Object;
	}

	static const ConstructorHelpers::FObjectFinder<UPropertyAnimatorEaseCurve> LinearCurve(TEXT("/Script/PropertyAnimator.PropertyAnimatorEaseCurve'/PropertyAnimator/Eases/Linear.Linear'"));

	if (LinearCurve.Succeeded())
	{
		EaseIn.EaseCurve = LinearCurve.Object;
		EaseOut.EaseCurve = LinearCurve.Object;
	}
}

void UPropertyAnimatorCurve::SetWaveCurve(UPropertyAnimatorWaveCurve* InCurve)
{
	WaveCurve = InCurve;
}

void UPropertyAnimatorCurve::SetEaseInEnabled(bool bInEnabled)
{
	bEaseInEnabled = bInEnabled;
}

void UPropertyAnimatorCurve::SetEaseIn(const FPropertyAnimatorCurveEasing& InEasing)
{
	EaseIn = InEasing;
}

void UPropertyAnimatorCurve::SetEaseOutEnabled(bool bInEnabled)
{
	bEaseOutEnabled = bInEnabled;
}

void UPropertyAnimatorCurve::SetEaseOut(const FPropertyAnimatorCurveEasing& InEasing)
{
	EaseOut = InEasing;
}

#if WITH_EDITOR
void UPropertyAnimatorCurve::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCurve, EaseIn))
	{
		OnEaseInChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCurve, EaseOut))
	{
		OnEaseOutChanged();
	}
}
#endif

bool UPropertyAnimatorCurve::EvaluateProperty(const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorCoreContext* InContext, FInstancedPropertyBag& InParameters, FInstancedPropertyBag& OutEvaluationResult) const
{
	if (!WaveCurve)
	{
		return false;
	}

	const FRichCurve& SampleCurve = WaveCurve->FloatCurve;

	float MinTime = 0;
	float MaxTime = 0;
	SampleCurve.GetTimeRange(MinTime, MaxTime);

	float MinValue = 0;
	float MaxValue = 0;
	SampleCurve.GetValueRange(MinValue, MaxValue);

	const double TimeElapsed = InParameters.GetValueDouble(TimeElapsedParameterName).GetValue();
	const float Frequency = InParameters.GetValueDouble(FrequencyParameterName).GetValue();
	const float Period = 1.f / Frequency;

	const float SampleTime = FMath::Fmod(TimeElapsed, Period);
	const float NormalizedSampleTime = FMath::GetMappedRangeValueClamped(FVector2D(0, Period), FVector2D(MinTime, MaxTime), SampleTime);

	const float SampleValue = SampleCurve.Eval(NormalizedSampleTime);
	float SampleValueNormalized = FMath::GetMappedRangeValueClamped(FVector2D(MinValue, MaxValue), FVector2D(0, 1), SampleValue);

	if (bEaseInEnabled && EaseIn.EaseCurve && SampleTime < EaseIn.EaseDuration)
	{
		const FRichCurve& EaseCurve = EaseIn.EaseCurve->FloatCurve;
		const float EaseTimeNormalized = FMath::GetMappedRangeValueClamped(FVector2D(0, EaseIn.EaseDuration), FVector2D(0, 1), SampleTime);
		SampleValueNormalized *= EaseCurve.Eval(EaseTimeNormalized);
	}

	if (bEaseOutEnabled && EaseOut.EaseCurve && SampleTime > (CycleDuration - EaseOut.EaseDuration))
	{
		const FRichCurve& EaseCurve = EaseOut.EaseCurve->FloatCurve;
		const float EaseTimeNormalized = 1 - FMath::GetMappedRangeValueClamped(FVector2D(CycleDuration - EaseOut.EaseDuration, CycleDuration), FVector2D(0, 1), SampleTime);
		SampleValueNormalized *= EaseCurve.Eval(EaseTimeNormalized);
	}

	InParameters.AddProperty(AlphaParameterName, EPropertyBagPropertyType::Float);
	InParameters.SetValueFloat(AlphaParameterName, SampleValueNormalized);

	return InContext->EvaluateProperty(InPropertyData, InParameters, OutEvaluationResult);
}

void UPropertyAnimatorCurve::OnEaseInChanged()
{
	EaseIn.EaseDuration = FMath::Clamp(EaseIn.EaseDuration, 0, CycleDuration - EaseOut.EaseDuration);
}

void UPropertyAnimatorCurve::OnEaseOutChanged()
{
	EaseOut.EaseDuration = FMath::Clamp(EaseOut.EaseDuration, 0, CycleDuration - EaseIn.EaseDuration);
}

void UPropertyAnimatorCurve::OnCycleDurationChanged()
{
	Super::OnCycleDurationChanged();

	OnEaseInChanged();
	OnEaseOutChanged();
}
