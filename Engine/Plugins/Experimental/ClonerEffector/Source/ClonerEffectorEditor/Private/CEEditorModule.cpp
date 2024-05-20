// Copyright Epic Games, Inc. All Rights Reserved.

#include "CEEditorModule.h"

#include "Cloner/CEClonerComponent.h"
#include "Cloner/CEClonerActor.h"
#include "Cloner/Customizations/CEEditorClonerComponentDetailCustomization.h"
#include "Cloner/Customizations/CEEditorClonerActorDetailCustomization.h"
#include "Effector/Customizations/CEEditorEffectorComponentDetailCustomization.h"
#include "Effector/Customizations/CEEditorEffectorTypeDetailCustomization.h"
#include "Effector/CEEffectorActor.h"
#include "Effector/CEEffectorComponent.h"
#include "Effector/Customizations/CEEditorEffectorActorDetailCustomization.h"
#include "Effector/Types/CEEffectorBoundType.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styles/CEEditorStyle.h"

void FCEEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);

	// Load styles
	FCEEditorStyle::Get();

	// Cloner customization
	PropertyModule.RegisterCustomClassLayout(ACEClonerActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorClonerActorDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCEClonerComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorClonerComponentDetailCustomization::MakeInstance));

	// Effector customization
	PropertyModule.RegisterCustomClassLayout(ACEEffectorActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorEffectorActorDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCEEffectorComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorEffectorComponentDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCEEffectorBoundType::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorEffectorTypeDetailCustomization::MakeInstance));
}

void FCEEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(PropertyEditorName))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);

		// Cloner customization
		PropertyModule.UnregisterCustomClassLayout(ACEClonerActor::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UCEClonerComponent::StaticClass()->GetFName());

		// Effector customization
		PropertyModule.UnregisterCustomClassLayout(ACEEffectorActor::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UCEEffectorComponent::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UCEEffectorBoundType::StaticClass()->GetFName());
	}
}

IMPLEMENT_MODULE(FCEEditorModule, ClonerEffectorEditor)