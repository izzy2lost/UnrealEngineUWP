// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

struct NDIlib_v5;

class FNDIMediaModule : public IModuleInterface
{
public:
	/** Dynamically loaded function pointers for the NDI lib API.*/
	static const NDIlib_v5* NDILib;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool LoadModuleDependencies();

private:
	/** Handle to the NDI runtime dll. */
	void* NDILibHandle = nullptr;
};
