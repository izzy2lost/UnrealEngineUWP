// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubEditorModule.h"

#include "SLiveLinkHubEditorStatusBar.h"
#include "ToolMenus.h"

void FLiveLinkHubEditorModule::OnPostEngineInit()
{
	if (GEditor)
	{
		RegisterLiveLinkHubStatusBar();
	}
}

void FLiveLinkHubEditorModule::RegisterLiveLinkHubStatusBar()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.StatusBar.ToolBar"));

	FToolMenuSection& LiveLinkHubSection = Menu->AddSection(TEXT("LiveLinkHub"), FText::GetEmpty(), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

	LiveLinkHubSection.AddEntry(
		FToolMenuEntry::InitWidget(TEXT("LiveLinkHubStatusBar"), CreateLiveLinkHubWidget(), FText::GetEmpty(), true, false)
	);
}

void FLiveLinkHubEditorModule::UnregisterLiveLinkHubStatusBar()
{
	UToolMenus::UnregisterOwner(this);
}

TSharedRef<SWidget> FLiveLinkHubEditorModule::CreateLiveLinkHubWidget()
{
	return SNew(SLiveLinkHubEditorStatusBar);
}

IMPLEMENT_MODULE(FLiveLinkHubEditorModule, LiveLinkHubEditor);
