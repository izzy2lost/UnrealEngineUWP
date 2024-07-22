// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorClock.h"

#include "Misc/DateTime.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

UPropertyAnimatorClock::UPropertyAnimatorClock()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

void UPropertyAnimatorClock::SetDisplayFormat(const FString& InDisplayFormat)
{
	DisplayFormat = InDisplayFormat;
}

EPropertyAnimatorPropertySupport UPropertyAnimatorClock::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FStrProperty>())
	{
		return EPropertyAnimatorPropertySupport::Complete;
	}

	// Check if a converter supports the conversion
	if (UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::String);
		const FPropertyBagPropertyDesc PropertyTypeDesc("", InPropertyData.GetLeafProperty());

		if (AnimatorSubsystem->IsConversionSupported(AnimatorTypeDesc, PropertyTypeDesc))
		{
			return EPropertyAnimatorPropertySupport::Incomplete;
		}
	}

	return Super::IsPropertySupported(InPropertyData);
}

void UPropertyAnimatorClock::EvaluateProperties(FInstancedPropertyBag& InParameters)
{
	const double TimeElapsed = InParameters.GetValueDouble(TimeElapsedParameterName).GetValue();

	const FTimespan ElapsedTimeSpan = FTimespan::FromSeconds(TimeElapsed);
	const FDateTime DateTime(ElapsedTimeSpan > FTimespan::Zero() ? ElapsedTimeSpan.GetTicks() : 0);
	const FString FormattedDateTime = DateTime.ToFormattedString(*DisplayFormat);

	EvaluateEachLinkedProperty<UPropertyAnimatorCoreContext>([this, FormattedDateTime](
		UPropertyAnimatorCoreContext* InContext,
		const FPropertyAnimatorCoreData& InResolvedProperty,
		FInstancedPropertyBag& InEvaluatedValues)->bool
	{
		const FName DisplayName(InResolvedProperty.GetPathHash());

		InEvaluatedValues.AddProperty(DisplayName, EPropertyBagPropertyType::String);
		InEvaluatedValues.SetValueString(DisplayName, FormattedDateTime);

		return true;
	});
}

void UPropertyAnimatorClock::OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty, EPropertyAnimatorPropertySupport InSupport)
{
	Super::OnPropertyLinked(InLinkedProperty, InSupport);

	const FPropertyAnimatorCoreData& Property = InLinkedProperty->GetAnimatedProperty();
	if (Property.IsA<FStrProperty>())
	{
		return;
	}

	if (EnumHasAnyFlags(InSupport, EPropertyAnimatorPropertySupport::Incomplete))
	{
		if (const UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
		{
			static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::String);
			const FPropertyBagPropertyDesc PropertyTypeDesc("", Property.GetLeafProperty());
			const TSet<UPropertyAnimatorCoreConverterBase*> Converters = AnimatorSubsystem->GetSupportedConverters(AnimatorTypeDesc, PropertyTypeDesc);
			check(!Converters.IsEmpty())
			InLinkedProperty->SetConverterClass(Converters.Array()[0]->GetClass());
		}
	}
}