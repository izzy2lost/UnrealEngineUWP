// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorFloatBase.h"

#include "Properties/PropertyAnimatorFloatContext.h"
#include "Properties/PropertyAnimatorRotatorContext.h"
#include "Properties/PropertyAnimatorVectorContext.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

#if WITH_EDITOR
void UPropertyAnimatorFloatBase::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	static const FName RandomTimeOffsetName = GET_MEMBER_NAME_CHECKED(UPropertyAnimatorFloatBase, bRandomTimeOffset);
	static const FName SeedName = GET_MEMBER_NAME_CHECKED(UPropertyAnimatorFloatBase, Seed);

	if (MemberName == SeedName
		|| MemberName == RandomTimeOffsetName)
	{
		OnSeedChanged();
	}
}
#endif

void UPropertyAnimatorFloatBase::SetMagnitude(float InMagnitude)
{
	if (FMath::IsNearlyEqual(Magnitude, InMagnitude))
	{
		return;
	}

	Magnitude = InMagnitude;
	OnMagnitudeChanged();
}

void UPropertyAnimatorFloatBase::SetCycleDuration(float InCycleDuration)
{
	if (FMath::IsNearlyEqual(CycleDuration, InCycleDuration))
	{
		return;
	}

	CycleDuration = InCycleDuration;
	OnCycleDurationChanged();
}

void UPropertyAnimatorFloatBase::SetCycleMode(EPropertyAnimatorCycleMode InMode)
{
	if (CycleMode == InMode)
	{
		return;
	}

	CycleMode = InMode;
	OnCycleModeChanged();
}

void UPropertyAnimatorFloatBase::SetTimeOffset(double InOffset)
{
	if (FMath::IsNearlyEqual(TimeOffset, InOffset))
	{
		return;
	}

	TimeOffset = InOffset;
	OnTimeOffsetChanged();
}

void UPropertyAnimatorFloatBase::SetRandomTimeOffset(bool bInOffset)
{
	if (bRandomTimeOffset == bInOffset)
	{
		return;
	}

	bRandomTimeOffset = bInOffset;
	OnSeedChanged();
}

void UPropertyAnimatorFloatBase::SetSeed(int32 InSeed)
{
	if (Seed == InSeed)
	{
		return;
	}

	Seed = InSeed;
	OnSeedChanged();
}

TSubclassOf<UPropertyAnimatorCoreContext> UPropertyAnimatorFloatBase::GetPropertyContextClass(const FPropertyAnimatorCoreData& InProperty)
{
	if (InProperty.IsA<FStructProperty>())
	{
		const FName TypeName = InProperty.GetLeafPropertyTypeName();

		if (TypeName == NAME_Rotator)
		{
			return UPropertyAnimatorRotatorContext::StaticClass();
		}

		if (TypeName == NAME_Vector)
		{
			return UPropertyAnimatorVectorContext::StaticClass();
		}
	}

	return UPropertyAnimatorFloatContext::StaticClass();
}

EPropertyAnimatorPropertySupport UPropertyAnimatorFloatBase::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	const FName TypeName = InPropertyData.GetLeafPropertyTypeName();

	if (InPropertyData.IsA<FFloatProperty>())
	{
		return EPropertyAnimatorPropertySupport::Complete;
	}

	if (InPropertyData.IsA<FStructProperty>())
	{
		if (TypeName == NAME_Rotator)
		{
			return EPropertyAnimatorPropertySupport::Complete;
		}

		if (TypeName == NAME_Vector)
		{
			return EPropertyAnimatorPropertySupport::Complete;
		}
	}

	// Check if a converter supports the conversion
	if (UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::Float);
		const FPropertyBagPropertyDesc PropertyTypeDesc("", InPropertyData.GetLeafProperty());

		if (AnimatorSubsystem->IsConversionSupported(AnimatorTypeDesc, PropertyTypeDesc))
		{
			return EPropertyAnimatorPropertySupport::Incomplete;
		}
	}

	return Super::IsPropertySupported(InPropertyData);
}

void UPropertyAnimatorFloatBase::EvaluateProperties(FInstancedPropertyBag& InParameters)
{
	const float AnimatorMagnitude = Magnitude * InParameters.GetValueFloat(MagnitudeParameterName).GetValue();
	double TimeElapsed = InParameters.GetValueDouble(TimeElapsedParameterName).GetValue();
	RandomStream = FRandomStream(Seed);

	EvaluateEachLinkedProperty<UPropertyAnimatorCoreContext>([this, &TimeElapsed, &AnimatorMagnitude, &InParameters](
		UPropertyAnimatorCoreContext* InOptions
		, const FPropertyAnimatorCoreData& InResolvedProperty
		, FInstancedPropertyBag& InEvaluatedValues
		, int32 InRangeIndex
		, int32 InRangeMax)->bool
	{
		const double RandomTimeOffset = bRandomTimeOffset ? RandomStream.GetFraction() : 0;

		const double AbsTimeOffset = FMath::Abs(TimeOffset);
		const double MaxTimeOffset = InRangeMax * AbsTimeOffset;
		double PropertyTimeElapsed = TimeElapsed + RandomTimeOffset;

		if (TimeOffset >= 0)
		{
			PropertyTimeElapsed += InRangeIndex * AbsTimeOffset;
		}
		else
		{
			PropertyTimeElapsed += MaxTimeOffset - InRangeIndex * AbsTimeOffset;
		}

		if (CycleMode == EPropertyAnimatorCycleMode::DoOnce)
		{
			if (FMath::Abs(PropertyTimeElapsed) > CycleDuration)
			{
				return false;
			}
		}
		else if (CycleMode == EPropertyAnimatorCycleMode::Loop)
		{
			PropertyTimeElapsed = FMath::Fmod(PropertyTimeElapsed, CycleDuration + MaxTimeOffset + CycleGapDuration);

			if (FMath::Abs(PropertyTimeElapsed) > CycleDuration)
			{
				PropertyTimeElapsed = CycleDuration - UE_KINDA_SMALL_NUMBER;
			}
		}
		else if (CycleMode == EPropertyAnimatorCycleMode::PingPong)
		{
			const bool bReverse = FMath::Modulo(FMath::TruncToInt32(PropertyTimeElapsed / (CycleDuration + MaxTimeOffset + CycleGapDuration)), 2) != 0;
			PropertyTimeElapsed = FMath::Fmod(PropertyTimeElapsed, CycleDuration + MaxTimeOffset + CycleGapDuration);

			if (FMath::Abs(PropertyTimeElapsed) > CycleDuration)
			{
				PropertyTimeElapsed = CycleDuration - UE_KINDA_SMALL_NUMBER;
			}

			if (bReverse)
			{
				PropertyTimeElapsed = CycleDuration - FMath::Fmod(PropertyTimeElapsed, CycleDuration);
			}
			else
			{
				PropertyTimeElapsed = FMath::Fmod(PropertyTimeElapsed, CycleDuration);
			}
		}

		if (Magnitude != 0
			&& CycleDuration > 0
			&& InOptions->GetMagnitude() != 0)
		{
			// Frequency
			InParameters.AddProperty(FrequencyParameterName, EPropertyBagPropertyType::Float);
			InParameters.SetValueFloat(FrequencyParameterName, 1.f / CycleDuration);

			// Time Elapsed
			InParameters.SetValueDouble(TimeElapsedParameterName, PropertyTimeElapsed);

			// Magnitude
			InParameters.SetValueFloat(MagnitudeParameterName, AnimatorMagnitude * InOptions->GetMagnitude());

			return EvaluateProperty(InResolvedProperty, InOptions, InParameters, InEvaluatedValues);
		}

		return false;
	});
}

void UPropertyAnimatorFloatBase::OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty, EPropertyAnimatorPropertySupport InSupport)
{
	Super::OnPropertyLinked(InLinkedProperty, InSupport);

	if (EnumHasAnyFlags(InSupport, EPropertyAnimatorPropertySupport::Incomplete))
	{
		if (const UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
		{
			static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::Float);
			const FPropertyBagPropertyDesc PropertyTypeDesc("", InLinkedProperty->GetAnimatedProperty().GetLeafProperty());

			const TSet<UPropertyAnimatorCoreConverterBase*> Converters = AnimatorSubsystem->GetSupportedConverters(AnimatorTypeDesc, PropertyTypeDesc);

			check(!Converters.IsEmpty())

			InLinkedProperty->SetConverterClass(Converters.Array()[0]->GetClass());
		}
	}
}
