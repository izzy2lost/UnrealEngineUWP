// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorCoreContext.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "StructUtils/InstancedStruct.h"
#include "Properties/PropertyAnimatorCoreResolver.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPropertyAnimatorCoreContext, Log, All);

TArray<FPropertyAnimatorCoreData> UPropertyAnimatorCoreContext::ResolveProperty(bool bInForEvaluation) const
{
	TArray<FPropertyAnimatorCoreData> ResolvedProperties;

	if (UPropertyAnimatorCoreResolver* PropertyResolver = GetResolver())
	{
		PropertyResolver->ResolveProperties(AnimatedProperty, ResolvedProperties, bInForEvaluation);
	}
	else
	{
		ResolvedProperties.Add(AnimatedProperty);
	}

	return ResolvedProperties;
}

FName UPropertyAnimatorCoreContext::GetAnimatedPropertyName()
{
	return GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, AnimatedProperty);
}

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreContext::GetAnimator() const
{
	return GetTypedOuter<UPropertyAnimatorCoreBase>();
}

UPropertyAnimatorCoreHandlerBase* UPropertyAnimatorCoreContext::GetHandler() const
{
	if (!HandlerWeak.IsValid())
	{
		if (const UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
		{
			UPropertyAnimatorCoreContext* MutableThis = const_cast<UPropertyAnimatorCoreContext*>(this);
			MutableThis->HandlerWeak = AnimatorSubsystem->GetHandler(AnimatedProperty);
		}
	}

	return HandlerWeak.Get();
}

UPropertyAnimatorCoreResolver* UPropertyAnimatorCoreContext::GetResolver() const
{
	if (Resolver)
	{
		return Resolver;
	}

	return AnimatedProperty.GetPropertyResolver();
}

bool UPropertyAnimatorCoreContext::IsResolvable() const
{
	return AnimatedProperty.IsResolvable();
}

bool UPropertyAnimatorCoreContext::IsConverted() const
{
	return !!ConverterClass.Get();
}

void UPropertyAnimatorCoreContext::SetAnimated(bool bInAnimated)
{
	if (bAnimated == bInAnimated)
	{
		return;
	}

	bAnimated = bInAnimated;
	OnAnimatedChanged();
}

void UPropertyAnimatorCoreContext::SetMagnitude(float InMagnitude)
{
	Magnitude = FMath::Clamp(InMagnitude, 0.f, 1.f);
}

void UPropertyAnimatorCoreContext::SetTimeOffset(double InOffset)
{
	TimeOffset = InOffset;
}

void UPropertyAnimatorCoreContext::SetMode(EPropertyAnimatorCoreMode InMode)
{
	if (InMode == Mode)
	{
		return;
	}

	Restore();
	Mode = InMode;
	OnModeChanged();
}

void UPropertyAnimatorCoreContext::SetConverterClass(TSubclassOf<UPropertyAnimatorCoreConverterBase> InConverterClass)
{
	ConverterClass = InConverterClass;

	if (const UPropertyAnimatorCoreConverterBase* Converter = InConverterClass.GetDefaultObject())
	{
		if (UScriptStruct* RuleStruct = Converter->GetConversionRuleStruct())
		{
			ConverterRule = FInstancedStruct(RuleStruct);
			CheckEditConverterRule();
		}
	}
}

void UPropertyAnimatorCoreContext::PostLoad()
{
	Super::PostLoad();

	CheckEditMode();
	CheckEditConverterRule();
	CheckEditResolver();

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float InDelta)
	{
		// Restore before regenerating new property path
		Restore();

		if (AnimatedProperty.IsResolvable())
		{
			DeltaPropertyValues.Reset();
			OriginalPropertyValues.Reset();
		}

		AnimatedProperty.GeneratePropertyPath();

		return false;
	}));
}

#if WITH_EDITOR
void UPropertyAnimatorCoreContext::PreEditChange(FProperty* InPropertyAboutToChange)
{
	Super::PreEditChange(InPropertyAboutToChange);

	if (!InPropertyAboutToChange)
	{
		return;
	}

	const FName MemberName = InPropertyAboutToChange->GetFName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreContext, Mode))
	{
		Restore();
	}
}

void UPropertyAnimatorCoreContext::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreContext, Mode))
	{
		OnModeChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreContext, bAnimated))
	{
		OnAnimatedChanged();
	}
}
#endif

bool UPropertyAnimatorCoreContext::ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue)
{
	TSharedPtr<FJsonObject>* JsonObject;
	if (!InValue->TryGetObject(JsonObject) || !JsonObject || !JsonObject->IsValid())
	{
		return false;
	}

	bool bJsonAnimated = bAnimated;
	(*JsonObject)->TryGetBoolField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, bAnimated), bJsonAnimated);
	SetAnimated(bJsonAnimated);

	if (bEditMagnitude)
	{
		float JsonMagnitude = Magnitude;
		(*JsonObject)->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Magnitude), JsonMagnitude);
		SetMagnitude(JsonMagnitude);

		float JsonTimeOffset = TimeOffset;
		(*JsonObject)->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, TimeOffset), JsonTimeOffset);
		SetTimeOffset(JsonTimeOffset);
	}

	if (bEditMode)
	{
		float JsonMode = static_cast<float>(Mode);
		(*JsonObject)->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Mode), JsonMode);
		SetMode(static_cast<EPropertyAnimatorCoreMode>(JsonMode));
	}

	if (Resolver)
	{
		if ((*JsonObject)->HasTypedField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Resolver), EJson::Object))
		{
			Resolver->ImportPreset(InPreset, (*JsonObject)->TryGetField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Resolver)).ToSharedRef());
		}
	}

	return true;
}

bool UPropertyAnimatorCoreContext::ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue)
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	OutValue = MakeShared<FJsonValueObject>(JsonObject);

	JsonObject->SetBoolField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, bAnimated), bAnimated);

	if (bEditMagnitude)
	{
		JsonObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Magnitude), Magnitude);
		JsonObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, TimeOffset), TimeOffset);
	}

	if (bEditMode)
	{
		JsonObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Mode), static_cast<float>(Mode));
	}

	JsonObject->SetStringField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, AnimatedProperty), AnimatedProperty.GetPropertyLocatorPath());

	if (Resolver)
	{
		TSharedPtr<FJsonValue> JsonValue;
		if (Resolver->ExportPreset(InPreset, JsonValue) && JsonValue.IsValid())
		{
			JsonObject->SetField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreContext, Resolver), JsonValue);
		}
	}

	return true;
}

void UPropertyAnimatorCoreContext::OnAnimatedPropertyLinked()
{
	bEditMagnitude = AnimatedProperty.IsA<FNumericProperty>() || AnimatedProperty.HasA<FNumericProperty>();

	if (const UPropertyAnimatorCoreResolver* PropertyResolver = AnimatedProperty.GetPropertyResolver())
	{
		if (!PropertyResolver->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Transient))
		{
			bEditResolver = true;
			Resolver = NewObject<UPropertyAnimatorCoreResolver>(this, PropertyResolver->GetClass());
		}
	}
}

void UPropertyAnimatorCoreContext::OnModeChanged()
{
	if (const UPropertyAnimatorCoreHandlerBase* Handler = GetHandler())
	{
		if (Mode == EPropertyAnimatorCoreMode::Additive && !Handler->IsAdditiveSupported())
		{
			Mode = EPropertyAnimatorCoreMode::Absolute;
		}

		Save();
	}
}

bool UPropertyAnimatorCoreContext::ResolvePropertyOwner(AActor* InNewOwner)
{
	UObject* NewOwner = InNewOwner ? InNewOwner : GetTypedOuter<AActor>();
	const UObject* CurrentOwner = AnimatedProperty.GetOwningActor();

	if (CurrentOwner == NewOwner)
	{
		return true;
	}

	bool bFound = IsValid(NewOwner);
	const TArray<UObject*> OtherOuters = AnimatedProperty.GetOuters(AnimatedProperty.GetOwningActor());

	const FProperty* MemberProperty = AnimatedProperty.GetMemberProperty();
	UClass* PropertyOwningClass = MemberProperty->GetOwnerClass();

	if (!OtherOuters.IsEmpty())
	{
		// Resolve using outers
		for (const UObject* OtherOuter : OtherOuters)
		{
			bFound = false;
			TArray<UObject*> ThisOwnedObjects;
			GetObjectsWithOuter(NewOwner, ThisOwnedObjects, false);

			// Search for the same class and same name
			for (UObject* ThisOuter : ThisOwnedObjects)
			{
				if (ThisOuter->GetClass() == OtherOuter->GetClass()
					&& ThisOuter->GetFName() == OtherOuter->GetFName())
				{
					bFound = true;
					NewOwner = ThisOuter;
					break;
				}
			}

			// Search for the property owning class
			if (!bFound)
			{
				for (UObject* ThisOuter : ThisOwnedObjects)
				{
					if (ThisOuter->GetClass() == OtherOuter->GetClass()
						|| ThisOuter->IsA(PropertyOwningClass))
					{
						bFound = true;
						NewOwner = ThisOuter;
						break;
					}
				}

				// Nothing found, stop searching
				if (!bFound)
				{
					break;
				}
			}
		}
	}
	else
	{
		// Resolve using path segments
		const TArray<FString> OwnerSegments = AnimatedProperty.GetOuterNames();

		for (const FString& PathSegment : OwnerSegments)
		{
			bFound = false;
			TArray<UObject*> ThisOwnedObjects;
			GetObjectsWithOuter(NewOwner, ThisOwnedObjects, false);

			for (UObject* ThisOuter : ThisOwnedObjects)
			{
				if (ThisOuter && ThisOuter->GetName().StartsWith(PathSegment))
				{
					bFound = true;
					NewOwner = ThisOuter;
					break;
				}
			}

			// Nothing found, stop searching
			if (!bFound)
			{
				break;
			}
		}
	}

	if (bFound
		&& IsValid(NewOwner)
		&& NewOwner->GetClass()->IsChildOf(PropertyOwningClass)
		&& FindFProperty<FProperty>(NewOwner->GetClass(), AnimatedProperty.GetMemberPropertyName()))
	{
		SetAnimatedPropertyOwner(NewOwner);
		return true;
	}

	UE_LOG(LogPropertyAnimatorCoreContext, Warning, TEXT("Could not resolve property owner %s on %s"), *AnimatedProperty.GetPathHash(), NewOwner ? *NewOwner->GetName() : TEXT("Invalid"))

	return false;
}

void UPropertyAnimatorCoreContext::ConstructInternal(const FPropertyAnimatorCoreData& InProperty)
{
	AnimatedProperty = InProperty;
	CheckEditMode();
	CheckEditConverterRule();
	CheckEditResolver();
	SetMode(EPropertyAnimatorCoreMode::Additive);
	OnAnimatedPropertyLinked();
}

void UPropertyAnimatorCoreContext::SetAnimatedPropertyOwner(UObject* InNewOwner)
{
	if (!IsValid(InNewOwner))
	{
		return;
	}

	if (!FindFProperty<FProperty>(InNewOwner->GetClass(), AnimatedProperty.GetMemberPropertyName()))
	{
		return;
	}

	constexpr bool bEvenIfPendingKill = true;
	UObject* PreviousOwner = AnimatedProperty.GetOwnerWeak().Get(bEvenIfPendingKill);
	AnimatedProperty = FPropertyAnimatorCoreData(InNewOwner, AnimatedProperty.GetChainProperties(), AnimatedProperty.GetPropertyResolverClass());

	OnAnimatedPropertyOwnerUpdated(PreviousOwner, InNewOwner);
}

void UPropertyAnimatorCoreContext::CheckEditMode()
{
	if (const UPropertyAnimatorCoreHandlerBase* Handler = GetHandler())
	{
		bEditMode = Handler->IsAdditiveSupported();
	}
}

void UPropertyAnimatorCoreContext::CheckEditConverterRule()
{
	bEditConverterRule = ConverterRule.IsValid();
}

void UPropertyAnimatorCoreContext::CheckEditResolver()
{
	bEditResolver = AnimatedProperty.IsResolvable();
}

void* UPropertyAnimatorCoreContext::GetConverterRulePtr(const UScriptStruct* InStruct)
{
	if (ConverterRule.IsValid() && ConverterRule.GetScriptStruct()->IsChildOf(InStruct))
	{
		return ConverterRule.GetMutableMemory();
	}

	return nullptr;
}

void UPropertyAnimatorCoreContext::Restore()
{
	if (OriginalPropertyValues.GetNumPropertiesInBag() == 0
		&& DeltaPropertyValues.GetNumPropertiesInBag() == 0)
	{
		return;
	}

	UPropertyAnimatorCoreHandlerBase* Handler = GetHandler();

	if (!Handler)
	{
		return;
	}

	if (Mode == EPropertyAnimatorCoreMode::Absolute)
	{
		for (const FPropertyAnimatorCoreData& ResolvedProperty : ResolveProperty(false))
		{
			// Reset original value
			if (Handler->SetValue(ResolvedProperty, OriginalPropertyValues))
			{
				OriginalPropertyValues.RemovePropertyByName(FName(ResolvedProperty.GetPathHash()));
			}
		}

		OriginalPropertyValues.Reset();
	}
	else
	{
		for (const FPropertyAnimatorCoreData& ResolvedProperty : ResolveProperty(false))
		{
			// Subtract delta value
			if (Handler->SubtractValue(ResolvedProperty, DeltaPropertyValues))
			{
				DeltaPropertyValues.RemovePropertyByName(FName(ResolvedProperty.GetPathHash()));
			}
		}
	}

	DeltaPropertyValues.Reset();
}

void UPropertyAnimatorCoreContext::Save()
{
	UPropertyAnimatorCoreHandlerBase* Handler = GetHandler();

	if (!Handler)
	{
		return;
	}

	for (const FPropertyAnimatorCoreData& PropertyData : ResolveProperty(false))
	{
		const FName Name(PropertyData.GetPathHash());
		if (!OriginalPropertyValues.FindPropertyDescByName(Name))
		{
			const FProperty* Property = PropertyData.GetLeafProperty();
			OriginalPropertyValues.AddProperty(Name, Property);

			// Save original value
			Handler->GetValue(PropertyData, OriginalPropertyValues);
		}

		if (!DeltaPropertyValues.FindPropertyDescByName(Name))
		{
			const FProperty* Property = PropertyData.GetLeafProperty();
			DeltaPropertyValues.AddProperty(Name, Property);
		}
	}
}

void UPropertyAnimatorCoreContext::OnAnimatedChanged()
{
	if (!bAnimated)
	{
		Restore();
	}
}

void UPropertyAnimatorCoreContext::CommitEvaluationResult(const FPropertyAnimatorCoreData& InResolvedProperty, const FInstancedPropertyBag& InEvaluatedValues)
{
	if (!IsAnimated())
	{
		return;
	}

	UPropertyAnimatorCoreHandlerBase* Handler = GetHandler();

	if (!Handler)
	{
		return;
	}

	const FName PropertyName(InResolvedProperty.GetPathHash());

	const FPropertyBagPropertyDesc* FromProperty = InEvaluatedValues.FindPropertyDescByName(PropertyName);
	const FPropertyBagPropertyDesc* ToProperty = DeltaPropertyValues.FindPropertyDescByName(PropertyName);

	if (UPropertyAnimatorCoreConverterBase* Converter = ConverterClass.GetDefaultObject())
	{
		if (!Converter->Convert(*FromProperty, InEvaluatedValues, *ToProperty, DeltaPropertyValues, ConverterRule.IsValid() ? &ConverterRule : nullptr))
		{
			return;
		}
	}
	else
	{
		// Ids need to match for copy to be successful
		const_cast<FPropertyBagPropertyDesc*>(FromProperty)->ID = ToProperty->ID;

		DeltaPropertyValues.CopyMatchingValuesByID(InEvaluatedValues);
	}

	if (Mode == EPropertyAnimatorCoreMode::Absolute)
	{
		Handler->SetValue(InResolvedProperty, DeltaPropertyValues);
		DeltaPropertyValues.RemovePropertyByName(PropertyName);
	}
	else
	{
		Handler->AddValue(InResolvedProperty, DeltaPropertyValues);
	}
}
