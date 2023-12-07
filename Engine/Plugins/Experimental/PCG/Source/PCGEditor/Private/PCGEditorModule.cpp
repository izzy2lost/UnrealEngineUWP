// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorModule.h"

#include "PCGEditorCommands.h"
#include "PCGEditorGraphNodeFactory.h"
#include "PCGEditorSettings.h"
#include "PCGEditorStyle.h"
#include "PCGEditorUtils.h"
#include "PCGSubsystem.h"
#include "PCGVolumeFactory.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Details/EnumSelectorDetails.h"
#include "Details/PCGAttributePropertySelectorDetails.h"
#include "Details/PCGBlueprintSettingsDetails.h"
#include "Details/PCGGraphDetails.h"
#include "Details/PCGGraphInstanceDetails.h"
#include "Details/PCGInstancedPropertyBagOverrideDetails.h"
#include "Details/PCGVolumeDetails.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FPCGEditorModule"

void FPCGEditorModule::StartupModule()
{
	RegisterDetailsCustomizations();
	RegisterMenuExtensions();
	RegisterSettings();

	FPCGEditorCommands::Register();
	FPCGEditorStyle::Register();

	GraphNodeFactory = MakeShareable(new FPCGEditorGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);

	if (GEditor)
	{
		GEditor->ActorFactories.Add(NewObject<UPCGVolumeFactory>());
	}

	if (!IsRunningCommandlet())
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FPCGEditorModule::RegisterOnEditorModeChange);
	}
}

void FPCGEditorModule::ShutdownModule()
{
	UnregisterSettings();
	UnregisterDetailsCustomizations();
	UnregisterMenuExtensions();

	FPCGEditorCommands::Unregister();
	FPCGEditorStyle::Unregister();

	FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);

	if (GEditor)
	{
		GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UPCGVolumeFactory>(); });
	}

	if (!IsRunningCommandlet())
	{
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);
		if (GLevelEditorModeToolsIsValid())
		{
			GLevelEditorModeTools().OnEditorModeIDChanged().RemoveAll(this);
		}
	}
}

void FPCGEditorModule::OnEditorModeIDChanged(const FEditorModeID& EditorModeID, bool bIsEntering)
{
	if (EditorModeID == FBuiltinEditorModes::EM_Landscape && !bIsEntering)
	{
		if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
		{
			PCGSubsystem->NotifyLandscapeEditModeExited();
		}
	}
}

bool FPCGEditorModule::SupportsDynamicReloading()
{
	return true;
}

void FPCGEditorModule::RegisterOnEditorModeChange()
{
	// Have a callback that catches changes in the Editor modes, to catch when we exit the landscape edit mode.
	if (!IsRunningCommandlet() && GLevelEditorModeToolsIsValid())
	{
		GLevelEditorModeTools().OnEditorModeIDChanged().AddRaw(this, &FPCGEditorModule::OnEditorModeIDChanged);
	}
}

void FPCGEditorModule::RegisterDetailsCustomizations()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomClassLayout("PCGBlueprintSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGBlueprintSettingsDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("PCGComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGComponentDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("PCGGraph", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGGraphDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("PCGGraphInstance", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGGraphInstanceDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("PCGVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGVolumeDetails::MakeInstance));

	PropertyEditor.RegisterCustomPropertyTypeLayout("PCGAttributePropertySelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGAttributePropertySelectorDetails::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout("PCGAttributePropertyInputSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGAttributePropertySelectorDetails::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout("PCGAttributePropertyOutputSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGAttributePropertySelectorDetails::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout("PCGAttributePropertyOutputNoSourceSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGAttributePropertySelectorDetails::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout("PCGOverrideInstancedPropertyBag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGOverrideInstancedPropertyBagDetails::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout("EnumSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEnumSelectorDetails::MakeInstance));

	PropertyEditor.NotifyCustomizationModuleChanged();
}

void FPCGEditorModule::UnregisterDetailsCustomizations()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomClassLayout("PCGBlueprintSettings");
		PropertyModule->UnregisterCustomClassLayout("PCGComponent");
		PropertyModule->UnregisterCustomClassLayout("PCGGraph");
		PropertyModule->UnregisterCustomClassLayout("PCGGraphInstance");
		PropertyModule->UnregisterCustomClassLayout("PCGVolume");

		PropertyModule->UnregisterCustomPropertyTypeLayout("PCGAttributePropertySelector");
		PropertyModule->UnregisterCustomPropertyTypeLayout("PCGAttributePropertyInputSelector");
		PropertyModule->UnregisterCustomPropertyTypeLayout("PCGAttributePropertyOutputSelector");
		PropertyModule->UnregisterCustomPropertyTypeLayout("PCGAttributePropertyOutputNoSourceSelector");
		PropertyModule->UnregisterCustomPropertyTypeLayout("PCGOverrideInstancedPropertyBag");

		PropertyModule->NotifyCustomizationModuleChanged();
	}
}

void FPCGEditorModule::RegisterMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->AddSection("PCGToolsSection", LOCTEXT("PCGToolsSection", "Procedural Generation Tools"));

	Section.AddSubMenu(
		"PCGToolsSubMenu",
		LOCTEXT("PCGSubMenu", "PCG Framework"),
		LOCTEXT("PCGSubMenu_Tooltip", "PCG Framework related functionality"),
		FNewMenuDelegate::CreateRaw(this, &FPCGEditorModule::PopulateMenuActions));
}

void FPCGEditorModule::UnregisterMenuExtensions()
{
	UToolMenus::UnregisterOwner(this);
}

void FPCGEditorModule::PopulateMenuActions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeletePCGPartitionActors", "Delete all PCG partition actors"),
		LOCTEXT("DeletePCGPartitionActors_Tooltip", "Deletes all PCG partition actors in the current world"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->DeletePartitionActors(/*bOnlyDeleteUnused=*/false);
					}
				}
			})),
		NAME_None);	
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeletePCGPartitionActorsChildren", "Delete all PCG partition actors children"),
		LOCTEXT("DeletePCGPartitionActorsChildren_Tooltip", "Deletes all PCG partition actors children in the current world, but not the Partition Actors themselves"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->DeletePartitionActors(/*bOnlyDeleteUnused=*/false, /*bOnlyChildren=*/true);
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteUnusedPCGPartitionActors", "Delete all unused PCG partition actors"),
		LOCTEXT("DeleteUnusedPCGPartitionActors_Tooltip", "Deletes all PCG partition actors in the current world that doesn't intersect with any PCG Component."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->DeletePartitionActors(/*bOnlyDeleteUnused=*/true);
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeletePCGWorldActor", "Deletes all PCG World Actors"),
		LOCTEXT("DeletePCGWorldActor_Tooltip", "Deletes all PCG World Actors"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->DestroyAllPCGWorldActors();
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BuildLandscapeCache", "Build Landscape Data Cache"),
		LOCTEXT("BuildLandscapeCache_Tooltip", "Caches the landscape data in the PCG World Actor"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->BuildLandscapeCache();
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearLandscapeCache", "Clear Landscape Data Cache"),
		LOCTEXT("ClearLandscapeCache_Tooltip", "Clears the landscape data cache in the PCG World Actor"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->ClearLandscapeCache();
					}
				}
				})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CancelAllGeneration", "Cancel all PCG tasks"),
		LOCTEXT("CancelAllGeneration_Tooltip", "Cancels all PCG tasks running"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->CancelAllGeneration();
					}
				}
				})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("UpdatePCGBlueprintVariableVisibility", "Make all PCG blueprint variables visible to instances"),
		LOCTEXT("UpdatePCGBlueprintVariableVisibility_Tooltip", "Will visit all PCG blueprints, update their Instance editable flag, unless there is already one variable that is visible"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				PCGEditorUtils::ForcePCGBlueprintVariableVisibility();
				})),
		NAME_None);

	MenuBuilder.AddSubMenu(
		LOCTEXT("PCGToolsLoggingSubMenu", "Logging / Reporting"),
		LOCTEXT("PCGToolsLoggingSubMenu_Tooltip", "Logging and reporting related editor commands"),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& LoggingMenuBuilder)
		{
			LoggingMenuBuilder.AddMenuEntry(
			LOCTEXT("LogAbnormalComponentState", "Log abnormal component state (actor order)"),
			LOCTEXT("LogAbnormalComponentState_Tooltip", "Logs unusual PCG components state, for every loaded actor"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([]() {
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->LogAbnormalComponentStates(/*bGroupByState=*/false);
					}
				})),
			NAME_None);

			LoggingMenuBuilder.AddMenuEntry(
			LOCTEXT("LogAbnormalComponentState_GroupedByState", "Log abnormal component state (grouped by state)"),
			LOCTEXT("LogAbnormalComponentState_GroupedByState_Tooltip", "Logs unusual PCG components, for every loaded actor, grouped by state"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([]() {
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
					{
						PCGSubsystem->LogAbnormalComponentStates(/*bGroupByState=*/true);
					}
				})),
			NAME_None);
		}),
		false,
		FSlateIcon());

}

void FPCGEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "ContentEditors", "PCGEditor",
			LOCTEXT("PCGEditorSettingsName", "PCG Editor"),
			LOCTEXT("PCGEditorSettingsDescription", "Configure the look and feel of the PCG Editor."),
			GetMutableDefault<UPCGEditorSettings>());
	}
}

void FPCGEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "PCGEditor");
	}
}

IMPLEMENT_MODULE(FPCGEditorModule, PCGEditor);

DEFINE_LOG_CATEGORY(LogPCGEditor);

#undef LOCTEXT_NAMESPACE
