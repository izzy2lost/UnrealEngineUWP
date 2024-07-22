// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorVectorContext.h"

#include "Animators/PropertyAnimatorCoreBase.h"

void UPropertyAnimatorVectorContext::SetAmplitudeMin(const FVector& InAmplitude)
{
	AmplitudeMin = GetClampedAmplitude(InAmplitude);
}

void UPropertyAnimatorVectorContext::SetAmplitudeMax(const FVector& InAmplitude)
{
	AmplitudeMax = GetClampedAmplitude(InAmplitude);
}

#if WITH_EDITOR
void UPropertyAnimatorVectorContext::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberPropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorVectorContext, AmplitudeMin))
	{
		SetAmplitudeMin(AmplitudeMin);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorVectorContext, AmplitudeMax))
	{
		SetAmplitudeMax(AmplitudeMax);
	}
}
#endif

bool UPropertyAnimatorVectorContext::EvaluateProperty(const FPropertyAnimatorCoreData& InProperty, const FInstancedPropertyBag& InAnimatorResult, FInstancedPropertyBag& OutEvaluatedValues)
{
	const TValueOrError<float, EPropertyBagResult> AlphaResult = InAnimatorResult.GetValueFloat(UPropertyAnimatorCoreBase::AlphaParameterName);
	const TValueOrError<float, EPropertyBagResult> MagnitudeResult = InAnimatorResult.GetValueFloat(UPropertyAnimatorCoreBase::MagnitudeParameterName);

	if (AlphaResult.HasValue())
	{
		const FName DisplayName(InProperty.GetPathHash());
		OutEvaluatedValues.AddProperty(DisplayName, EPropertyBagPropertyType::Struct, TBaseStructure<FVector>::Get());
		OutEvaluatedValues.SetValueStruct(DisplayName, MagnitudeResult.GetValue() * FMath::Lerp(AmplitudeMin, AmplitudeMax, AlphaResult.GetValue()));
		return true;
	}

	return false;
}

void UPropertyAnimatorVectorContext::OnAnimatedPropertyLinked()
{
	Super::OnAnimatedPropertyLinked();

	AmplitudeClampMin.Reset();
	AmplitudeClampMax.Reset();

#if WITH_EDITOR
	const FPropertyAnimatorCoreData& Property = GetAnimatedProperty();
	const FProperty* LeafProperty = Property.GetLeafProperty();

	checkf(LeafProperty, TEXT("Animated leaf property must be valid"))

	// Assign Min and Max value based on editor meta data available
	if (LeafProperty->IsA<FStructProperty>())
	{
		if (LeafProperty->HasMetaData(TEXT("ClampMin")))
		{
			AmplitudeMin = FVector(LeafProperty->GetFloatMetaData(FName("ClampMin")));
			AmplitudeClampMin = AmplitudeMin;
		}
		else if (LeafProperty->HasMetaData(TEXT("UIMin")))
		{
			AmplitudeMin = FVector(LeafProperty->GetFloatMetaData(FName("UIMin")));
		}

		if (LeafProperty->HasMetaData(TEXT("ClampMax")))
		{
			AmplitudeMax = FVector(LeafProperty->GetFloatMetaData(FName("ClampMax")));
			AmplitudeClampMax = AmplitudeMax;
		}
		else if (LeafProperty->HasMetaData(TEXT("UIMax")))
		{
			AmplitudeMax = FVector(LeafProperty->GetFloatMetaData(FName("UIMax")));
		}
	}
#endif
}

FVector UPropertyAnimatorVectorContext::GetClampedAmplitude(FVector InAmplitude)
{
	if (AmplitudeClampMin.IsSet())
	{
		const FVector& MinAmplitude = AmplitudeClampMin.GetValue();
		InAmplitude = InAmplitude.ComponentMax(MinAmplitude);
	}

	if (AmplitudeClampMax.IsSet())
	{
		const FVector& MaxAmplitude = AmplitudeClampMax.GetValue();
		InAmplitude = InAmplitude.ComponentMin(MaxAmplitude);
	}

	return InAmplitude;
}
