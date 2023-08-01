// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSettings.h"
#include "MassLWISettings.generated.h"


#define GET_MASSLWI_CONFIG_VALUE(a) (GetMutableDefault<UMassLWISettings>()->a)

/**
 * Implements the settings for MassLWI plugin
 */
UCLASS(config = Mass, defaultconfig, DisplayName = "Mass LWI")
class MASSLWI_API UMassLWISettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
	bool IsMassLWIEnabled() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = Mass, config)
	bool bMassLWIEnabled = true;
};
