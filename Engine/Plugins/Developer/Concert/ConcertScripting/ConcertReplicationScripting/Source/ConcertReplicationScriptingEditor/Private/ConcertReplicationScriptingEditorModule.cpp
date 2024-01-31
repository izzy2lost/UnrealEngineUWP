// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertReplicationScriptingEditorModule.h"

#include "ConcertPropertyChainWrapper.h"
#include "ReplicationScriptingStyle.h"
#include "Customization/ConcertPropertyCustomization.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

namespace UE::ConcertReplicationScriptingEditor
{
	void FConcertReplicationScriptingEditorModule::StartupModule()
	{
		FReplicationScriptingStyle::Initialize();
		
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(
			FConcertPropertyChainWrapper::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FConcertPropertyCustomization::MakeInstance)
			);
	}

	void FConcertReplicationScriptingEditorModule::ShutdownModule()
	{
		FReplicationScriptingStyle::Shutdown();
	}
}

IMPLEMENT_MODULE(UE::ConcertReplicationScriptingEditor::FConcertReplicationScriptingEditorModule, ConcertReplicationScriptingEditor);