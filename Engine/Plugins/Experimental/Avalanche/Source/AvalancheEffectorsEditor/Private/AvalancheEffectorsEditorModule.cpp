// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheEffectorsEditorModule.h"
#include "AvaEffectorsEditorCommands.h"
#include "AvaEffectorsEditorStyle.h"
#include "AvaInteractiveToolsDelegates.h"
#include "AvalancheShapesEditorModule.h"
#include "Cloner/AvaClonerActor.h"
#include "Cloner/AvaClonerActorTool.h"
#include "Cloner/AvaClonerActorVis.h"
#include "Cloner/AvaClonerComponent.h"
#include "Cloner/AvaClonerDetailCustomization.h"
#include "ComponentVisualizers.h"
#include "Effector/AvaEffectorActorTool.h"
#include "Effector/AvaEffectorActorVis.h"
#include "Effector/AvaEffectorComponent.h"
#include "Effector/AvaEffectorDetailCustomization.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "AvalancheEffectorsEditorModule"

FAvalancheEffectorsEditorModule& FAvalancheEffectorsEditorModule::Get()
{
	static const FName ModuleName = TEXT("AvalancheEffectorsEditor");
	return FModuleManager::LoadModuleChecked<FAvalancheEffectorsEditorModule>(ModuleName);
}

void FAvalancheEffectorsEditorModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAvalancheEffectorsEditorModule::PostEngineInit);

	RegisterCustomLayouts();
	FAvaEffectorsEditorStyle::Get();
	FAvaEffectorsEditorCommands::Register();

	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().AddRaw(this, &FAvalancheEffectorsEditorModule::RegisterTools);
}

void FAvalancheEffectorsEditorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UnregisterCustomLayouts();
	}

	FAvaEffectorsEditorCommands::Unregister();
	
	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().RemoveAll(this);
}

void FAvalancheEffectorsEditorModule::PostEngineInit()
{
	if (FSlateApplication::IsInitialized())
	{
		RegisterComponentVisualizers();
	}
}

void FAvalancheEffectorsEditorModule::RegisterTools(IAvalancheInteractiveToolsModule* InModule)
{
	InModule->RegisterTool(
		IAvalancheInteractiveToolsModule::Get().CategoryNameActor,
		GetDefault<UAvaEffectorActorTool>()->GetToolParameters()
	);

	InModule->RegisterTool(
		IAvalancheInteractiveToolsModule::Get().CategoryNameActor,
		GetDefault<UAvaClonerActorTool>()->GetToolParameters()
	);
}

void FAvalancheEffectorsEditorModule::RegisterComponentVisualizers()
{
	IAvalancheComponentVisualizersModule::RegisterComponentVisualizer<UAvaEffectorComponent, FAvaEffectorActorVisualizer>(&Visualizers);
	IAvalancheComponentVisualizersModule::RegisterComponentVisualizer<UAvaClonerComponent, FAvaClonerActorVisualizer>(&Visualizers);
}

namespace UE::AvalancheEffectorsEditor::Private
{
	static const FName PropertyEditorModuleName("PropertyEditor");
}

void FAvalancheEffectorsEditorModule::RegisterCustomLayouts()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(UE::AvalancheEffectorsEditor::Private::PropertyEditorModuleName);

	// Cloner/effector
	PropertyModule.RegisterCustomClassLayout(AAvaEffectorActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FAvaEffectorDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(AAvaClonerActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FAvaClonerDetailCustomization::MakeInstance));
}

void FAvalancheEffectorsEditorModule::UnregisterCustomLayouts()
{
	if (FModuleManager::Get().IsModuleLoaded(UE::AvalancheEffectorsEditor::Private::PropertyEditorModuleName))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(UE::AvalancheEffectorsEditor::Private::PropertyEditorModuleName);

		// Cloner/effector
		PropertyModule.UnregisterCustomClassLayout(AAvaEffectorActor::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(AAvaClonerActor::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvalancheEffectorsEditorModule, AvalancheEffectorsEditorModule)
