// Copyright Epic Games, Inc. All Rights Reserved.

#include "PPMChainGraphEditorModule.h"
#include "Modules/ModuleManager.h"


void FPPMChainGraphEditorModule::StartupModule()
{
}

void FPPMChainGraphEditorModule::ShutdownModule()
{
	
}

IMPLEMENT_MODULE(FPPMChainGraphEditorModule, PPMChainGraphEditor);
DEFINE_LOG_CATEGORY(LogPPMChainGraphEditor);
