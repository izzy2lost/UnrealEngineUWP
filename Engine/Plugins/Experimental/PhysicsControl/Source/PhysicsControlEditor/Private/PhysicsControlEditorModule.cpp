// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlEditorModule.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlComponentVisualizer.h"
#include "OperatorEditor/OperatorEditor.h"
#include "PhysicsControlProfileAssetActions.h"
#include "PhysicsControlProfileEditorMode.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Features/IModularFeatures.h"

static const FName PhysicsControlEditorModule_PhysicsControlEditorInterface("PhysicsControlEditorInterface");

#define LOCTEXT_NAMESPACE "PhysicsControlModule"

//======================================================================================================================
void FPhysicsControlEditorModule::StartupModule()
{
#ifdef ENABLE_PHYSICS_CONTROL_PROFILE_ASSET
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	PhysicsControlProfileAssetActions = MakeShared<FPhysicsControlProfileAssetActions>();
	AssetTools.RegisterAssetTypeActions(PhysicsControlProfileAssetActions.ToSharedRef());
#endif

#if ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR
	FEditorModeRegistry::Get().RegisterMode<FPhysicsControlProfileEditorMode>(
		FPhysicsControlProfileEditorMode::ModeName, 
		LOCTEXT("PhysicsControlProfileEditorMode", "PhysicsControlProfile"), 
		FSlateIcon(), false);
#endif

	if (GUnrealEd)
	{
		TSharedPtr<FPhysicsControlComponentVisualizer> Visualizer = MakeShared<FPhysicsControlComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UPhysicsControlComponent::StaticClass()->GetFName(), Visualizer);
		// This call should maybe be inside the RegisterComponentVisualizer call above, but since it's not,
		// we'll put it here.
		Visualizer->OnRegister();
		VisualizersToUnregisterOnShutdown.Add(UPhysicsControlComponent::StaticClass()->GetFName());
	}

	if (!EditorInterface)
	{
		EditorInterface = new FPhysicsControlOperatorEditor;
	}

	if (EditorInterface)
	{
		EditorInterface->Startup();
		IModularFeatures::Get().RegisterModularFeature(PhysicsControlEditorModule_PhysicsControlEditorInterface, EditorInterface);
	}
}

//======================================================================================================================
void FPhysicsControlEditorModule::ShutdownModule()
{
	if (EditorInterface)
	{
		EditorInterface->Shutdown();
		IModularFeatures::Get().UnregisterModularFeature(PhysicsControlEditorModule_PhysicsControlEditorInterface, EditorInterface);
		delete EditorInterface;
		EditorInterface = nullptr;
	}
	
	// Physics Control Profile editor/asset is disabled for now
#if ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR
	FEditorModeRegistry::Get().UnregisterMode(FPhysicsControlProfileEditorMode::ModeName);
#endif

#ifdef ENABLE_PHYSICS_CONTROL_PROFILE_ASSET
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(
			PhysicsControlProfileAssetActions.ToSharedRef());
	}
#endif

	if (GUnrealEd)
	{
		for (const FName& Name : VisualizersToUnregisterOnShutdown)
		{
			GUnrealEd->UnregisterComponentVisualizer(Name);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPhysicsControlEditorModule, PhysicsControlEditor)
