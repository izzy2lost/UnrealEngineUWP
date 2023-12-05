// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "Utility/CPSTimerManager.h"

class CAPTUREPROTOCOLSTACK_API FCaptureProtocolStackModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FCPSTimerManager& GetTimerManager();

private:

	TUniquePtr<FCPSTimerManager> TimerManager;
};
