// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#include "Templates/SharedPointerFwd.h"
#include "Framework/Commands/UICommandList.h"

class FAvalancheMaskEditorModule
	: public IModuleInterface
{
public:	
    // ~Begin IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    // ~End IModuleInterface

    TSharedPtr<FUICommandList> GetCommandList() const;
	
private:
	void RegisterMenus();	
	void ToggleEditorMode();

private:
	TSharedPtr<FUICommandList> CommandList;
};
