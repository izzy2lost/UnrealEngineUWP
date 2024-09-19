// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorCoreResolver.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

bool UPropertyAnimatorCoreResolver::ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue)
{
	const TSharedPtr<FJsonObject>* JsonTimeSourceObject;
	if (!InValue->TryGetObject(JsonTimeSourceObject) || !JsonTimeSourceObject)
	{
		return false;
	}

	return true;
}

bool UPropertyAnimatorCoreResolver::ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue)
{
	TSharedRef<FJsonObject> JsonTimeSourceObject = MakeShared<FJsonObject>();
	OutValue = MakeShared<FJsonValueObject>(JsonTimeSourceObject);

	return true;
}
