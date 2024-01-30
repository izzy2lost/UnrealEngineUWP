// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskEditorModule.h"

#include "AvaMaskEditorCommands.h"
#include "AvaMaskEditorMode.h"
#include "AvaMaskEditorStyle.h"
#include "AvaMaskEditorSubsystem.h"
#include "EditorModeManager.h"
#include "Templates/SharedPointer.h"
#include "ToolMenuEntry.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AvalancheMaskEditor"

void FAvalancheMaskEditorModule::StartupModule()
{
	FAvalancheMaskEditorStyle::Initialize();
	FAvaMaskEditorCommands::Register();

	CommandList = MakeShared<FUICommandList>();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAvalancheMaskEditorModule::RegisterMenus));
}

void FAvalancheMaskEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	
	FAvalancheMaskEditorStyle::Shutdown();
	FAvaMaskEditorCommands::Unregister();
}

TSharedPtr<FUICommandList> FAvalancheMaskEditorModule::GetCommandList() const
{
	return CommandList;
}

void FAvalancheMaskEditorModule::ToggleEditorMode()
{
	GLevelEditorModeTools().ActivateMode(UAvalancheMaskEditorMode::EM_AvalancheMaskEditorModeId, true);
}

void FAvalancheMaskEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FAvaMaskEditorCommands::Get().ToggleMaskMode, CommandList);
		}
	}

	// Bottom center viewport overlay when mode is active
	{
		static FName ToolkitOverlayMenuName = UE::AvalancheMaskEditor::Internal::ToolkitOverlayMenuName;

		FToolMenuContext MenuContext(CommandList);
		
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(ToolkitOverlayMenuName, NAME_None, EMultiBoxType::ToolBar, false);
		Menu->Context = MenuContext;
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("AvalancheLevelViewport.StatusBar"));
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("ModeToggles");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FAvaMaskEditorCommands::Get().ToggleMaskMode));				
				Entry.Label.Set(FText::GetEmpty());
				Entry.Icon.Set(FSlateIcon(FAvalancheMaskEditorStyle::GetStyleSetName(), TEXT("AvalancheMaskEditor.ToggleMaskMode.Small")));
				
				Entry.SetCommandList(CommandList);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvalancheMaskEditorModule, AvalancheMaskEditor)
