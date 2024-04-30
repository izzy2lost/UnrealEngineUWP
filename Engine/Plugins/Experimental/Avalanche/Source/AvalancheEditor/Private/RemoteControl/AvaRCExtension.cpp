// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRCExtension.h"
#include "AvaRCSignatureCustomization.h"
#include "Editor.h"
#include "Framework/Docking/LayoutExtender.h"
#include "IAvaSceneInterface.h"
#include "IRemoteControlUIModule.h"
#include "LevelEditor.h"
#include "RemoteControlPreset.h"
#include "RemoteControlTrackerComponent.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/RemoteControlComponentsSubsystem.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AvaRCExtension"

URemoteControlPreset* FAvaRCExtension::GetRemoteControlPreset() const
{
	const IAvaSceneInterface* const Scene = GetSceneObject<IAvaSceneInterface>();
	return Scene ? Scene->GetRemoteControlPreset() : nullptr;
}

void FAvaRCExtension::Activate()
{
	RegisterSignatureCustomization();
	OpenRemoteControlTab();
}

void FAvaRCExtension::Deactivate()
{
	CloseRemoteControlTab();
	UnregisterSignatureCustomization();
}

void FAvaRCExtension::ExtendLevelEditorLayout(FLayoutExtender& InExtender) const
{
	const FName RemoteControlTabId(TEXT("RemoteControl_RemoteControlPanel"));

	InExtender.ExtendLayout(LevelEditorTabIds::Sequencer
		, ELayoutExtensionPosition::Before
		, FTabManager::FTab(RemoteControlTabId, ETabState::ClosedTab));
}

void FAvaRCExtension::ExtendToolbarMenu(UToolMenu& InMenu)
{
	FToolMenuSection& Section = InMenu.FindOrAddSection(DefaultSectionName);

	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("RemoteControlButton")
		,  FExecuteAction::CreateSP(this, &FAvaRCExtension::OpenRemoteControlTab)
		, LOCTEXT("RemoteControlLabel"  , "Remote Control")
		, LOCTEXT("RemoteControlTooltip", "Opens the Remote Control Editor for the given Scene")
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details")));

	Entry.StyleNameOverride = "CalloutToolbar";
}

void FAvaRCExtension::OpenRemoteControlTab() const
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();
	if (!Editor.IsValid())
	{
		return;
	}

	TSharedPtr<IToolkitHost> ToolkitHost = Editor->GetToolkitHost();
	if (!ToolkitHost.IsValid())
	{
		return;
	}

	if (URemoteControlPreset* const RemoteControlPreset = GetRemoteControlPreset())
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		check(AssetEditorSubsystem);
		AssetEditorSubsystem->OpenEditorForAsset(RemoteControlPreset, EToolkitMode::WorldCentric, ToolkitHost);
	}
}

void FAvaRCExtension::CloseRemoteControlTab() const
{
	if (URemoteControlPreset* const RemoteControlPreset = GetRemoteControlPreset())
	{
		if (UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(RemoteControlPreset);
		}
	}
}

void FAvaRCExtension::RegisterSignatureCustomization()
{
	UnregisterSignatureCustomization();

	SignatureCustomization = MakeShared<FAvaRCSignatureCustomization>();
	IRemoteControlUIModule::Get().RegisterSignatureCustomization(SignatureCustomization);
}

void FAvaRCExtension::UnregisterSignatureCustomization()
{
	if (!SignatureCustomization.IsValid())
	{
		return;
	}

	if (IRemoteControlUIModule* RCUIModule = FModuleManager::GetModulePtr<IRemoteControlUIModule>(TEXT("RemoteControlUI")))
	{
		RCUIModule->UnregisterSignatureCustomization(SignatureCustomization);
	}

	SignatureCustomization.Reset();
}

#undef LOCTEXT_NAMESPACE
