// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectFixupToolModule.h"
#include "InstanceDataObjectFixupTool.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "InstanceDataObjectFixupToolModule"

static const FName InstanceDataObjectFixupToolTabName = FName(TEXT("InstanceDataObjectFixupTool"));
static const FName InstanceDataObjectFixupToolDialogName = FName(TEXT("LoosePropertyFixup"));

void FInstanceDataObjectFixupToolModule::StartupModule()
{
}

void FInstanceDataObjectFixupToolModule::ShutdownModule()
{
	
}

bool FInstanceDataObjectFixupToolModule::OpenInstanceDataObjectFixupTool() const
{
	TSharedPtr<SDockTab> DockTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(InstanceDataObjectFixupToolTabName));
	if (!DockTab)
	{
		return false;
	}
	DockTab->DrawAttention();
	return true;
}

TSharedRef<SDockTab> FInstanceDataObjectFixupToolModule::CreateInstanceDataObjectFixupTab(
	const FSpawnTabArgs& TabArgs, TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects, TObjectPtr<UObject> InstanceDataObjectsOwner) const
{
	const TSharedRef<SInstanceDataObjectFixupTool> InstanceDataObjectFixupTool = SNew(SInstanceDataObjectFixupTool)
		.InstanceDataObjects(InstanceDataObjects)
		.InstanceDataObjectsOwner(InstanceDataObjectsOwner);
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			InstanceDataObjectFixupTool
		];

	InstanceDataObjectFixupTool->SetDockTab(DockTab);
	InstanceDataObjectFixupTool->GenerateDetailsViews();
	
	return DockTab;
}

void FInstanceDataObjectFixupToolModule::CreateInstanceDataObjectFixupDialog(
	TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects, TObjectPtr<UObject> InstanceDataObjectsOwner) const
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(InstanceDataObjectFixupToolDialogName, FOnSpawnTab::CreateLambda(
		[&InstanceDataObjects, InstanceDataObjectsOwner](const FSpawnTabArgs& TabArgs)
		{
			return FInstanceDataObjectFixupToolModule::Get().CreateInstanceDataObjectFixupTab(TabArgs, InstanceDataObjects, InstanceDataObjectsOwner);
		}))
		.SetDisplayName(LOCTEXT("ObjectFixupTabTitle", "Object Fix-up"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	if (const TSharedPtr<SDockTab> DockTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(InstanceDataObjectFixupToolDialogName)))
	{
		DockTab->DrawAttention();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInstanceDataObjectFixupToolModule, InstanceDataObjectFixupTool)
