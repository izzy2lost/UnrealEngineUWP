// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IGeometryMaskModule.h"

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGeometryMask, Log, All);

class FGeometryMaskModule
	: public IGeometryMaskModule
{
public:
	// ~Begin IModuleInterface
	virtual void StartupModule() override;
	// ~End IModuleInterface
};
