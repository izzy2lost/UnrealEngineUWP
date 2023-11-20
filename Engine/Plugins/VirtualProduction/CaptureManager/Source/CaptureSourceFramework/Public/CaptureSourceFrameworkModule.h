// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "CaptureSourceManager.h"
#include "CaptureSourceDetailsCustomization.h"

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceFrameworkModule
	: public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FCaptureSourceManager& GetCaptureSourceManager();
	FCaptureSourceDetailsCustomization& GetCustomization();

private:

	TUniquePtr<FCaptureSourceManager> CaptureManager;
	TUniquePtr<FCaptureSourceDetailsCustomization> Customization;
};