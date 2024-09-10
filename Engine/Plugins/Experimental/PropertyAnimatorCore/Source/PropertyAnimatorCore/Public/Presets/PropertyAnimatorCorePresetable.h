// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Templates/SharedPointer.h"
#include "PropertyAnimatorCorePresetable.generated.h"

class FJsonValue;
class UPropertyAnimatorCorePresetBase;

UINTERFACE(MinimalAPI)
class UPropertyAnimatorCorePresetable : public UInterface
{
	GENERATED_BODY()
};

/** Preset interface for animator system */
class IPropertyAnimatorCorePresetable
{
	GENERATED_BODY()

public:
	/** Import a specific preset */
	PROPERTYANIMATORCORE_API virtual bool ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue) = 0;

	/** Export a specific preset */
	PROPERTYANIMATORCORE_API virtual bool ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue) = 0;
};