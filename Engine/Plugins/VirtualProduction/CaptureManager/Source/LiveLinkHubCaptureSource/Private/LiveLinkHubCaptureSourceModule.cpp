// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubCaptureSourceModule.h"

#include "CaptureSourceFrameworkModule.h"

#include "LiveLinkHubCaptureSourceFactory.h"

void FLiveLinkHubCaptureSourceModule::StartupModule()
{
	FCaptureSourceFrameworkModule& Module = FModuleManager::LoadModuleChecked<FCaptureSourceFrameworkModule>("CaptureSourceFramework");

	Module.GetCaptureSourceManager().RegisterCaptureSourceFactory(MakeUnique<FLiveLinkHubCaptureSourceFactory>());
}

void FLiveLinkHubCaptureSourceModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FLiveLinkHubCaptureSourceModule, LiveLinkHubCaptureSource)