// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArchetypeFixupToolModule.h"
#include "ArchetypeFixupTool.h"

#define LOCTEXT_NAMESPACE "ArchetypeFixupToolModule"

static const FName ArchetypeFixupToolTabName = FName(TEXT("ArchetypeFixupTool"));

void FArchetypeFixupToolModule::StartupModule()
{
}

void FArchetypeFixupToolModule::ShutdownModule()
{
	
}

bool FArchetypeFixupToolModule::OpenArchetypeFixupTool() const
{
	TSharedPtr<SDockTab> DockTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ArchetypeFixupToolTabName));
	if (!DockTab)
	{
		return false;
	}
	DockTab->DrawAttention();
	return true;
}

TSharedRef<SDockTab> FArchetypeFixupToolModule::CreateArchetypeFixupTab(const FSpawnTabArgs& TabArgs, TConstArrayView<TObjectPtr<UObject>> Archetypes) const
{
	const TSharedRef<SArchetypeFixupTool> ArchetypeFixupTool = SNew(SArchetypeFixupTool)
		.Archetypes(Archetypes);
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			ArchetypeFixupTool
		];

	ArchetypeFixupTool->SetDockTab(DockTab);
	ArchetypeFixupTool->GenerateDetailsViews();
	
	return DockTab;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArchetypeFixupToolModule, ArchetypeFixupTool)
