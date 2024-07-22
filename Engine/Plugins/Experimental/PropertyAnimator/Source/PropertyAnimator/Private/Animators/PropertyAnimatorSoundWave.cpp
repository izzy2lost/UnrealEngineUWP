// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorSoundWave.h"

#include "LoudnessNRT.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "Sound/SoundWave.h"

UPropertyAnimatorSoundWave::UPropertyAnimatorSoundWave()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

void UPropertyAnimatorSoundWave::SetSampledSoundWave(USoundWave* InSoundWave)
{
	if (SampledSoundWave == InSoundWave)
	{
		return;
	}

	SampledSoundWave = InSoundWave;
	OnSampledSoundWaveChanged();
}

void UPropertyAnimatorSoundWave::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
}

#if WITH_EDITOR
void UPropertyAnimatorSoundWave::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorSoundWave, SampledSoundWave))
	{
		OnSampledSoundWaveChanged();
	}
}
#endif // WITH_EDITOR

void UPropertyAnimatorSoundWave::OnSampledSoundWaveChanged()
{
	if (!AudioAnalyzer)
	{
		AudioAnalyzer = NewObject<ULoudnessNRT>(this);
	}

	AudioAnalyzer->Sound = SampledSoundWave;

#if WITH_EDITOR
	// Needed to analyse new audio sample
	FProperty* SoundProperty = FindFProperty<FProperty>(ULoudnessNRT::StaticClass(), GET_MEMBER_NAME_CHECKED(ULoudnessNRT, Sound));
	FPropertyChangedEvent PropertyChangedEvent(SoundProperty, EPropertyChangeType::ValueSet);
	AudioAnalyzer->PostEditChangeProperty(PropertyChangedEvent);
#endif
}

bool UPropertyAnimatorSoundWave::EvaluateProperty(const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorCoreContext* InContext, FInstancedPropertyBag& InParameters, FInstancedPropertyBag& OutEvaluationResult) const
{
	if (AudioAnalyzer && AudioAnalyzer->DurationInSeconds > 0)
	{
		double TimeElapsed = InParameters.GetValueDouble(TimeElapsedParameterName).GetValue();
		float Frequency = InParameters.GetValueFloat(FrequencyParameterName).GetValue();

		const float Period = 1.f / Frequency;
		float SampleTime = FMath::Fmod(TimeElapsed, Period);

		if (FMath::Abs(TimeElapsed / Period) <= 1.f || bLoop)
		{
			if (SampleTime < 0)
			{
				SampleTime = Period - FMath::Abs(SampleTime);
			}

			const float NormalizedSampleTime = FMath::GetMappedRangeValueClamped(FVector2D(0, Period), FVector2D(0, AudioAnalyzer->DurationInSeconds), SampleTime);

			float NormalizedLoudness = 0.f;
			AudioAnalyzer->GetNormalizedLoudnessAtTime(NormalizedSampleTime, NormalizedLoudness);

			InParameters.AddProperty(AlphaParameterName, EPropertyBagPropertyType::Float);
			InParameters.SetValueFloat(AlphaParameterName, NormalizedLoudness);

			return InContext->EvaluateProperty(InPropertyData, InParameters, OutEvaluationResult);
		}
	}

	return false;
}
