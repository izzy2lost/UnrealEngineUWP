// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorCorePresetBase.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Presets/PropertyAnimatorCorePresetable.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

FString UPropertyAnimatorCorePresetBase::GetPresetDisplayName() const
{
	return FName::NameToDisplayString(PresetName.ToString(), false);
}

void UPropertyAnimatorCorePresetBase::CreatePreset(FName InName, const TArray<IPropertyAnimatorCorePresetable*>& InPresetableItem)
{
	PresetName = InName;
}
