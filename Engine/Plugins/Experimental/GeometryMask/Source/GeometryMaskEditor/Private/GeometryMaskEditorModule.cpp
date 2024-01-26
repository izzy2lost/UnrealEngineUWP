// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskEditorModule.h"

#include "Framework/Docking/TabManager.h"
#include "Materials/Material.h"
#include "Styling/SlateIconFinder.h"
#include "ViewModels/GMECanvasListViewModel.h"
#include "ViewModels/GMEResourceListViewModel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SGMECanvasList.h"
#include "Widgets/SGMEResourceList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FGeometryMaskEditorModule"

const FName FGeometryMaskEditorModule::VisualizerTabId = TEXT("GeometryMaskVisualizer");

void FGeometryMaskEditorModule::StartupModule()
{
	const FText TabText = LOCTEXT("GeometryMaskVisualizerTabName", "Geometry Mask");
	TSharedRef<FGlobalTabmanager> GlobalTabManager = FGlobalTabmanager::Get();

	GlobalTabManager->RegisterNomadTabSpawner(
			VisualizerTabId,
			FOnSpawnTab::CreateLambda(
				[this](const FSpawnTabArgs&)
				{
					TSharedRef<FGMECanvasListViewModel> CanvasListViewModel = FGMECanvasListViewModel::Create();
					TSharedRef<FGMEResourceListViewModel> ResourceListViewModel = FGMEResourceListViewModel::Create();

					TSharedRef<SDockTab> DockTab =
						SNew(SDockTab)
						.TabRole(ETabRole::NomadTab)
						.Content()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(0.5)
							[
								SNew(SGMECanvasList, CanvasListViewModel)
							]
							
							+ SHorizontalBox::Slot()
							.FillWidth(0.5)
							[
								SNew(SGMEResourceList, ResourceListViewModel)
							]
						];

					return DockTab;
				}
			)
		)
		.SetDisplayNameAttribute(TabText)
		.SetDisplayName(TabText)
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
		.SetIcon(FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()))
		.SetAutoGenerateMenuEntry(false); // Only show via console command
	
	if (GIsEditor && !IsRunningCommandlet())
	{
		RegisterConsoleCommands();
	}
}

void FGeometryMaskEditorModule::ShutdownModule()
{
	const TSharedRef<FGlobalTabmanager> GlobalTabManager = FGlobalTabmanager::Get();
	GlobalTabManager->UnregisterNomadTabSpawner(VisualizerTabId);
	VisualizerTabWeak.Reset();
	
	// Cleanup commands
	UnregisterConsoleCommands();
}

void FGeometryMaskEditorModule::RegisterConsoleCommands()
{
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("GeometryMask.Visualize"),
		TEXT("Displays a window listing all active GeometryMaskCanvas objects"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FGeometryMaskEditorModule::ExecuteShowVisualizer),
		ECVF_Default
	));
}

void FGeometryMaskEditorModule::UnregisterConsoleCommands()
{
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}

	ConsoleCommands.Empty();
}

void FGeometryMaskEditorModule::ExecuteShowVisualizer(const TArray<FString>& InArgs)
{
	if (!VisualizerTabWeak.IsValid())
	{
		VisualizerTabWeak = FGlobalTabmanager::Get()->TryInvokeTab(VisualizerTabId); 
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGeometryMaskEditorModule, GeometryMaskEditor)
