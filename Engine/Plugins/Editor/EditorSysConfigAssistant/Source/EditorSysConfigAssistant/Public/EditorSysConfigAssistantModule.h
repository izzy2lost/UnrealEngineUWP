// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IEditorSysConfigAssistantModule : public IModuleInterface
{
public:
	static FORCEINLINE IEditorSysConfigAssistantModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IEditorSysConfigAssistantModule>("EditorSysConfigAssistant");
	}
	
	/** Ensures the system configuration assistant UI is presented to the user */
	virtual void ShowSystemConfigAssistant() = 0;
};

