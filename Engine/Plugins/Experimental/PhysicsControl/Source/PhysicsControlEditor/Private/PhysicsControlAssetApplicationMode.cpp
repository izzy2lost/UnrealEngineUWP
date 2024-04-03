// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetApplicationMode.h"
#include "PhysicsControlAssetEditorToolkit.h"
#include "PersonaModule.h"
#include "Modules/ModuleManager.h"
#include "PersonaTabs.h"

#define LOCTEXT_NAMESPACE "PhysicsControlAssetApplicationMode"

FName FPhysicsControlAssetApplicationMode::ModeName("PhysicsControlAssetEditMode");

//======================================================================================================================
FPhysicsControlAssetApplicationMode::FPhysicsControlAssetApplicationMode(
	TSharedRef<FWorkflowCentricApplication> InHostingApp,
	TSharedRef<IPersonaPreviewScene>        InPreviewScene)
	: 
	FApplicationMode(PhysicsControlAssetEditorModes::Editor)
{
	EditorToolkit = StaticCastSharedRef<FPhysicsControlAssetEditorToolkit>(InHostingApp);
	TSharedRef<FPhysicsControlAssetEditorToolkit> PhysicsControlAssetEditor = 
		StaticCastSharedRef<FPhysicsControlAssetEditorToolkit>(InHostingApp);

	FPersonaViewportArgs ViewportArgs(InPreviewScene);
	ViewportArgs.bAlwaysShowTransformToolbar = true;
	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowTimeline = false;
	ViewportArgs.bShowLODMenu = true;
	ViewportArgs.bShowPlaySpeedMenu = false;
	ViewportArgs.bShowPhysicsMenu = false;
	ViewportArgs.ContextName = TEXT("PhysicsControlAssetEditor.Viewport");
	ViewportArgs.OnViewportCreated = FOnViewportCreated::CreateSP(
		PhysicsControlAssetEditor, &FPhysicsControlAssetEditorToolkit::HandleViewportCreated);

	// Register Persona tabs.
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InHostingApp, ViewportArgs));
	TabFactories.RegisterFactory(PersonaModule.CreateDetailsTabFactory(InHostingApp, FOnDetailsCreated::CreateSP(&PhysicsControlAssetEditor.Get(), &FPhysicsControlAssetEditorToolkit::HandleDetailsCreated)));

	// Create tab layout.
	TabLayout = FTabManager::NewLayout("Standalone_PhysicsControlAssetEditor_Layout_v0.001")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.32f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(1.0f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.7f)
						->SetHideTabWell(true)
						->AddTab(FPersonaTabs::PreviewViewportID, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(FPersonaTabs::DetailsID, ETabState::OpenedTab)
					->SetForegroundTab(FPersonaTabs::DetailsID)
				)
			)
		);

	PersonaModule.OnRegisterTabs().Broadcast(TabFactories, InHostingApp);
	LayoutExtender = MakeShared<FLayoutExtender>();
	PersonaModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender.Get());
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

//======================================================================================================================
void FPhysicsControlAssetApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FPhysicsControlAssetEditorToolkit> Editor = EditorToolkit.Pin();
	Editor->RegisterTabSpawners(InTabManager.ToSharedRef());
	Editor->PushTabFactories(TabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

#undef LOCTEXT_NAMESPACE
