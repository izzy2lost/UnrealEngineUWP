// Copyright Epic Games, Inc. All Rights Reserved.

#include "HierarchyTableBuiltinEditorModule.h"
#include "HierarchyTableTypeRegistry.h"
#include "TimeProfile/HierarchyTableTypeTime.h"
#include "TimeProfile/TimeProfileTypeHandler.h"
#include "MaskProfile/HierarchyTableTypeMask.h"
#include "MaskProfile/MaskProfileTypeHandler.h"
#include "Modules/ModuleManager.h"

void FHierarchyTableBuiltinEditorModule::StartupModule()
{
	UHierarchyTableTypeRegistry* Registry = GetMutableDefault<UHierarchyTableTypeRegistry>();
	Registry->Register(FHierarchyTableType_Mask::StaticStruct(), GetDefault<UHierarchyTableTypeHandler_Mask>());
	Registry->Register(FHierarchyTableType_Time::StaticStruct(), GetDefault<UHierarchyTableTypeHandler_Time>());
}

void FHierarchyTableBuiltinEditorModule::ShutdownModule()
{
	UHierarchyTableTypeRegistry* Registry = GetMutableDefault<UHierarchyTableTypeRegistry>();
	Registry->Unregister(FHierarchyTableType_Mask::StaticStruct());
	Registry->Unregister(FHierarchyTableType_Time::StaticStruct());
}

IMPLEMENT_MODULE(FHierarchyTableBuiltinEditorModule, HierarchyTableBuiltinEditor)
