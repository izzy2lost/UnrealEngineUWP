// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceFrameworkModule.h"

void FCaptureSourceFrameworkModule::StartupModule()
{
	CaptureManager = MakeUnique<FCaptureSourceManager>();
	Customization = MakeUnique<FCaptureSourceDetailsCustomization>();
}

void FCaptureSourceFrameworkModule::ShutdownModule()
{
	for (FCaptureSourceId Id : CaptureManager->GetCaptureSourceIds())
	{
		CaptureManager->RemoveCaptureSource(Id);
	}

	CaptureManager = nullptr;

	Customization = nullptr;
}

FCaptureSourceManager& FCaptureSourceFrameworkModule::GetCaptureSourceManager()
{
	return *CaptureManager;
}

FCaptureSourceDetailsCustomization& FCaptureSourceFrameworkModule::GetCustomization()
{
	return *Customization;
}

IMPLEMENT_MODULE(FCaptureSourceFrameworkModule, CaptureSourceFramework)