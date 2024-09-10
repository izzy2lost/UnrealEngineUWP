// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorCorePropertyPreset.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Properties/PropertyAnimatorCoreContext.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UPropertyAnimatorCorePropertyPreset::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	for (const TPair<FString, TSharedRef<FJsonValue>>& PropertyPreset : PropertyPresets)
	{
		FPropertyAnimatorCoreData Property(const_cast<AActor*>(InActor), PropertyPreset.Key);

		if (Property.IsResolved())
		{
			OutProperties.Add(Property);
		}
	}
}

void UPropertyAnimatorCorePropertyPreset::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
{
	for (const TPair<FString, TSharedRef<FJsonValue>>& PropertyPreset : PropertyPresets)
	{
		FPropertyAnimatorCoreData Property(InAnimator->GetAnimatorActor(), PropertyPreset.Key);

		if (!Property.IsResolved())
		{
			continue;
		}

		bool bFound = InProperties.Contains(Property);

		if (!bFound)
		{
			for (const FPropertyAnimatorCoreData& LinkedProperty : InProperties)
			{
				if (LinkedProperty.IsChildOf(Property))
				{
					Property = LinkedProperty;
					bFound = true;
					break;
				}
			}
		}

		if (bFound)
		{
			if (UPropertyAnimatorCoreContext* Context = InAnimator->GetLinkedPropertyContext(Property))
			{
				Context->ImportPreset(this, PropertyPreset.Value);
			}
		}
	}
}

void UPropertyAnimatorCorePropertyPreset::GetSupportedPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	TSet<FPropertyAnimatorCoreData> PresetProperties;
	GetPresetProperties(InActor, InAnimator, PresetProperties);
	OutProperties.Empty(PresetProperties.Num());

	if (PresetProperties.IsEmpty())
	{
		return;
	}

	for (const FPropertyAnimatorCoreData& PresetProperty : PresetProperties)
	{
		InAnimator->GetPropertiesSupported(PresetProperty, OutProperties, /** SearchDepth */3);
	}
}

bool UPropertyAnimatorCorePropertyPreset::IsPresetSupported(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator) const
{
	if (!IsValid(InActor) || !IsValid(InAnimator))
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InActor, InAnimator, SupportedProperties);

	return !SupportedProperties.IsEmpty();
}

bool UPropertyAnimatorCorePropertyPreset::ApplyPreset(UPropertyAnimatorCoreBase* InAnimator)
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, SupportedProperties);

	if (SupportedProperties.IsEmpty())
	{
		return false;
	}

	for (FPropertyAnimatorCoreData& SupportedProperty : SupportedProperties)
	{
		InAnimator->LinkProperty(SupportedProperty);
	}

	InAnimator->SetAnimatorDisplayName(FName(InAnimator->GetAnimatorOriginalName().ToString() + TEXT("_") + GetPresetDisplayName()));

	OnPresetApplied(InAnimator, SupportedProperties);

	return true;
}

bool UPropertyAnimatorCorePropertyPreset::IsPresetApplied(const UPropertyAnimatorCoreBase* InAnimator) const
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, SupportedProperties);

	if (SupportedProperties.IsEmpty())
	{
		return false;
	}

	return InAnimator->IsPropertiesLinked(SupportedProperties);
}

bool UPropertyAnimatorCorePropertyPreset::UnapplyPreset(UPropertyAnimatorCoreBase* InAnimator)
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, SupportedProperties);

	if (SupportedProperties.IsEmpty())
	{
		return false;
	}

	for (FPropertyAnimatorCoreData& SupportedProperty : SupportedProperties)
	{
		InAnimator->UnlinkProperty(SupportedProperty);
	}

	OnPresetUnapplied(InAnimator, SupportedProperties);

	return true;
}

void UPropertyAnimatorCorePropertyPreset::CreatePreset(FName InName, const TArray<IPropertyAnimatorCorePresetable*>& InPresetableItems)
{
	Super::CreatePreset(InName, InPresetableItems);

	TArray<TSharedPtr<FJsonValue>> JsonValues;

	for (IPropertyAnimatorCorePresetable* InPresetableItem : InPresetableItems)
	{
		TSharedPtr<FJsonValue> JsonValue;

		if (InPresetableItem
			&& InPresetableItem->ExportPreset(this, JsonValue)
			&& JsonValue.IsValid())
		{
			JsonValues.Add(JsonValue);
		}
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	if (FJsonSerializer::Serialize(JsonValues, JsonWriter))
	{
		PresetVersion = 0;
		PresetContent = JsonString;
	}
}

bool UPropertyAnimatorCorePropertyPreset::LoadPreset()
{
	if (PresetContent.IsEmpty())
	{
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PresetContent);

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	if (!FJsonSerializer::Deserialize(Reader, JsonArray) || JsonArray.IsEmpty())
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& JsonValue : JsonArray)
	{
		TSharedPtr<FJsonObject>* JsonObject = nullptr;
		if (!JsonValue
			|| !JsonValue->TryGetObject(JsonObject)
			|| !JsonObject)
		{
			continue;
		}

		FString PropertyPath;

		if ((*JsonObject)->TryGetStringField(UPropertyAnimatorCoreContext::GetAnimatedPropertyName().ToString(), PropertyPath)
			&& !PropertyPath.IsEmpty())
		{
			PropertyPresets.Add(PropertyPath, JsonValue.ToSharedRef());
		}
	}

	return !PropertyPresets.IsEmpty();
}

void UPropertyAnimatorCorePropertyPreset::GetAppliedPresetProperties(const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutSupportedProperties, TSet<FPropertyAnimatorCoreData>& OutAppliedProperties)
{
	OutSupportedProperties.Empty();
	OutAppliedProperties.Empty();

	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return;
	}

	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, OutSupportedProperties);
	OutAppliedProperties.Reserve(OutSupportedProperties.Num());

	for (const FPropertyAnimatorCoreData& Property : OutSupportedProperties)
	{
		if (InAnimator->IsPropertyLinked(Property))
		{
			OutAppliedProperties.Add(Property);
		}
	}
}
