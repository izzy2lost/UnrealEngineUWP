// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskEditorModule.h"

#include "AvaMaskEditorCommands.h"
#include "AvaMaskEditorMode.h"
#include "AvaMaskEditorSubsystem.h"
#include "Details/AvaMask2DModifierDetails.h"
#include "EditorModeManager.h"
#include "Mask2D/AvaMask2DReadModifier.h"
#include "Mask2D/AvaMask2DWriteModifier.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"
#include "ToolMenuEntry.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AvalancheMaskEditor"

void FAvalancheMaskEditorModule::StartupModule()
{
	// Details
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FOnGetDetailCustomizationInstance MaskCustomization = FOnGetDetailCustomizationInstance::CreateStatic(&FAvaMask2DModifierDetails::MakeInstance);
		PropertyModule.RegisterCustomClassLayout(UAvaMask2DReadModifier::StaticClass()->GetFName(), MaskCustomization);
		PropertyModule.RegisterCustomClassLayout(UAvaMask2DWriteModifier::StaticClass()->GetFName(), MaskCustomization);
		
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	FAvaMaskEditorCommands::Register();

	CommandList = MakeShared<FUICommandList>();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAvalancheMaskEditorModule::RegisterMenus));
}

void FAvalancheMaskEditorModule::ShutdownModule()
{
	// Details
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyModule->UnregisterCustomClassLayout(UAvaMask2DReadModifier::StaticClass()->GetFName());
			PropertyModule->UnregisterCustomClassLayout(UAvaMask2DWriteModifier::StaticClass()->GetFName());
			PropertyModule->NotifyCustomizationModuleChanged();
		}
	}
	
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FAvaMaskEditorCommands::Unregister();
}

TSharedPtr<FUICommandList> FAvalancheMaskEditorModule::GetCommandList() const
{
	return CommandList;
}

void FAvalancheMaskEditorModule::ToggleEditorMode()
{
	GLevelEditorModeTools().ActivateMode(UAvaMaskEditorMode::EM_MotionDesignMaskEditorModeId, true);
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
				Entry.Icon.Set(FSlateIcon(FAvaMaskEditorStyle::Get().GetStyleSetName(), TEXT("AvaMaskEditor.ToggleMaskMode.Small")));
				
				Entry.SetCommandList(CommandList);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvalancheMaskEditorModule, AvalancheMaskEditor)
