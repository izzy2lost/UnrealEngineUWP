// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FStormSyncAvaPlaylistExtender;

class FStormSyncAvaBridgeEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	/** UI extender for Storm Sync view and commands. */
	TSharedPtr<FStormSyncAvaPlaylistExtender> PlaylistExtender;
};
