// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlViewportModule.h"
#include "SourceControlViewportOutlineMenu.h"

void FSourceControlViewportModule::StartupModule()
{
	ViewportOutlineMenu = MakeShared<FSourceControlViewportOutlineMenu>();
	ViewportOutlineMenu->Init();
}

void FSourceControlViewportModule::ShutdownModule()
{
	ViewportOutlineMenu.Reset();
}

IMPLEMENT_MODULE( FSourceControlViewportModule, SourceControlViewport );