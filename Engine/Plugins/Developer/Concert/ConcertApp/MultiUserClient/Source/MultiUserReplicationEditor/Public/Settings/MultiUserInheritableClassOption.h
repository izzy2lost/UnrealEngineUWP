// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserInheritableClassOption.generated.h"

/** Base struct for settings intended for classes */
USTRUCT()
struct FMultiUserInheritableClassOption
{
	GENERATED_BODY()

	/** Whether to inherit the settings from the base class. */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bInheritFromBase = true;
};
