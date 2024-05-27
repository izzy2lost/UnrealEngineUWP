// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialTextureSetEditorModule.h"

#include "DMTextureSetContentBrowserIntegration.h"
#include "DMTextureSetStyle.h"
#include "Modules/ModuleManager.h"

void FDynamicMaterialTextureSetEditorModule::StartupModule()
{
	FDMTextureSetStyle::Get();
	FDMTextureSetContentBrowserIntegration::Integrate();
}

void FDynamicMaterialTextureSetEditorModule::ShutdownModule()
{
	FDMTextureSetContentBrowserIntegration::Disintegrate();
}

IMPLEMENT_MODULE(FDynamicMaterialTextureSetEditorModule, DynamicMaterialTextureSetEditor)
