// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChimeraAssetEditor.h"
#include "EditorModeRegistry.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#define LOCTEXT_NAMESPACE "FChimeraEditorModule"

namespace UE::Chimera
{

class FChimeraEditorModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FChimeraEditorModule::StartupModule()
{
	if (GIsEditor && !IsRunningCommandlet())
	{
		// Register Ed Mode used by ChimeraAsset
		FEditorModeRegistry::Get().RegisterMode<FChimeraAssetEdMode>(FChimeraAssetEdMode::EdModeId, LOCTEXT("ChimeraAssetEdModeName", "ChimeraAsset"));
	}
}

void FChimeraEditorModule::ShutdownModule()
{
	// Unregister Ed Mode
	FEditorModeRegistry::Get().UnregisterMode(FChimeraAssetEdMode::EdModeId);
}

} // namespace UE::Chimera

IMPLEMENT_MODULE(UE::Chimera::FChimeraEditorModule, ChimeraEditor)

#undef LOCTEXT_NAMESPACE
