// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskModule.h"

#include "IGeometryMaskModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogGeometryMask);

#define LOCTEXT_NAMESPACE "FGeometryMaskModule"

IGeometryMaskModule& IGeometryMaskModule::Get()
{
	return FModuleManager::LoadModuleChecked<IGeometryMaskModule>(UE_MODULE_NAME);
}

void FGeometryMaskModule::StartupModule()
{
	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir(), TEXT("Shaders"));
	// @todo: uncomment when shaders implemented
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/GeometryMask"), PluginShaderDir);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGeometryMaskModule, GeometryMask)
