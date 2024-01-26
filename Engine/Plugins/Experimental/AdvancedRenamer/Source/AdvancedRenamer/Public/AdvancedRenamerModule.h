// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogARP, Log, All);

/**
 * Advanced Rename Panel Plugin - Easily bulk rename stuff!
 */
class ADVANCEDRENAMER_API FAdvancedRenamerModule : public IModuleInterface
{
public:	
	static FAdvancedRenamerModule& Get();

	//~ Begin IAdvancedRenamerModule interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IAdvancedRenamerModule interface
};

