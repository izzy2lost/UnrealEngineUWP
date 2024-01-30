// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandleContainer.h"
#include "UObject/Object.h"
#include "AvaSceneSettings.generated.h"

/** Object containing information about its Scene */
UCLASS(MinimalAPI)
class UAvaSceneSettings : public UObject
{
	GENERATED_BODY()

public:
	const FAvaTagHandleContainer& GetTagAttributes() const
	{
		return TagAttributes;
	}

	UFUNCTION()
	void SetTagAttributes(const FAvaTagHandleContainer& InTagAttributes);

private:
	/** Tags describing the Scene */
	UPROPERTY(EditAnywhere, Setter="SetTagAttributes", Category="Scene", meta=(AllowPrivateAccess="true"))
	FAvaTagHandleContainer TagAttributes;
};
