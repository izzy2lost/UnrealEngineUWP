// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAvaTest, Log, All);

/**
 * Avalanche Test Module
 */
class FAvaTestModule : public IModuleInterface
{
public:
	//~ Begin IAvaTestModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IAvaTestModule

private:
	void OnTestStart(FAutomationTestBase* InTest);
};
