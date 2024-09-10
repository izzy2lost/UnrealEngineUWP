// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorCoreAnimatorPreset.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Presets/PropertyAnimatorCorePresetable.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool UPropertyAnimatorCoreAnimatorPreset::IsPresetApplied(const UPropertyAnimatorCoreBase* InAnimator) const
{
	return false;
}

bool UPropertyAnimatorCoreAnimatorPreset::IsPresetSupported(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator) const
{
	return InAnimator && InAnimator->IsA(TargetAnimatorClass);
}

bool UPropertyAnimatorCoreAnimatorPreset::ApplyPreset(UPropertyAnimatorCoreBase* InAnimator)
{
	if (InAnimator->IsTemplate())
	{
		return false;
	}

	return InAnimator->ImportPreset(this, AnimatorPreset.ToSharedRef());
}

bool UPropertyAnimatorCoreAnimatorPreset::UnapplyPreset(UPropertyAnimatorCoreBase* InAnimator)
{
	return false;
}

void UPropertyAnimatorCoreAnimatorPreset::CreatePreset(FName InName, const TArray<IPropertyAnimatorCorePresetable*>& InPresetableItem)
{
	Super::CreatePreset(InName, InPresetableItem);

	TSharedPtr<FJsonValue> JsonValue;

	if (InPresetableItem[0]
		&& InPresetableItem[0]->ExportPreset(this, JsonValue)
		&& JsonValue.IsValid())
	{
		TSharedPtr<FJsonObject>* JsonObject = nullptr;
		if (JsonValue->TryGetObject(JsonObject))
		{
			FString JsonString;
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
			if (FJsonSerializer::Serialize(JsonObject->ToSharedRef(), JsonWriter))
			{
				PresetVersion = 0;
				PresetContent = JsonString;
			}
		}
	}
}

bool UPropertyAnimatorCoreAnimatorPreset::LoadPreset()
{
	if (PresetContent.IsEmpty())
	{
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PresetContent);

	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	FString Class;
	JsonObject->TryGetStringField(TEXT("AnimatorClass"), Class);

	if (UClass* AnimatorClass = LoadObject<UClass>(nullptr, *Class))
	{
		TargetAnimatorClass = AnimatorClass;
		AnimatorPreset = MakeShared<FJsonValueObject>(JsonObject);
		return true;
	}

	return false;
}

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreAnimatorPreset::GetAnimatorTemplate() const
{
	return TargetAnimatorClass.GetDefaultObject();
}
