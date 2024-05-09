// Copyright Epic Games, Inc. All Rights Reserved.

#include "CEEditorModule.h"

#include "Cloner/CEEditorClonerComponentDetailCustomization.h"
#include "Cloner/CEClonerComponent.h"
#include "Effector/CEEditorEffectorComponentDetailCustomization.h"
#include "Effector/CEEditorEffectorTypeDetailCustomization.h"
#include "Effector/CEEffectorComponent.h"
#include "Effector/Types/CEEffectorBoundType.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styles/CEEditorStyle.h"

void FCEEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);

	// Load styles
	FCEEditorStyle::Get();

	// Cloner/effector customization
	PropertyModule.RegisterCustomClassLayout(UCEClonerComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorClonerComponentDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCEEffectorComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorEffectorComponentDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCEEffectorBoundType::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorEffectorTypeDetailCustomization::MakeInstance));
}

void FCEEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(PropertyEditorName))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);

		// Cloner/effector
		PropertyModule.UnregisterCustomClassLayout(UCEClonerComponent::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UCEEffectorComponent::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UCEEffectorBoundType::StaticClass()->GetFName());
	}
}

IMPLEMENT_MODULE(FCEEditorModule, ClonerEffectorEditor)