// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"


class FAndroidSingleInstanceServiceEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AndroidSingleInstanceServiceRuntimeSettings.h"
#include "Modules/ModuleManager.h"
#endif
