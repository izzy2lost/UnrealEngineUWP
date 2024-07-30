// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"

class FHierarchyTableEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	
	virtual void ShutdownModule() override;

private:
	TSharedPtr<class FHierarchyTableAssetTypeActions> HierarchyTableAssetTypeActions;
};