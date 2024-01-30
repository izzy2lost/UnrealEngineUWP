// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointerFwd.h"
#include "AvaViewportCameraHistory.h"

DECLARE_LOG_CATEGORY_EXTERN(AvaLevelViewportLog, Log, All);

namespace UE::AvaLevelViewport::Internal
{
	static FName StatusBarMenuName = TEXT("AvalancheLevelViewport.StatusBar");
}

class FAvalancheLevelViewportModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	void RegisterMenus();

protected:
	FDelegateHandle AvaLevelViewportClientCasterDelegateHandle;

	/** Handles viewport camera undo/redo */
	TSharedPtr<FAvaViewportCameraHistory> ViewportCameraHistory;
};
