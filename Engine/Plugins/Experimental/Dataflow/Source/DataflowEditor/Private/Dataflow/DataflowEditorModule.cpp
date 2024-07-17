// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModule.h"

#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEngineRendering.h"
#include "Dataflow/DataflowFunctionsProperty.h"
#include "Dataflow/DataflowFunctionsPropertyCustomization.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "Dataflow/ScalarVertexPropertyGroupCustomization.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"

#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "DataflowEditor"

const FColor FDataflowEditorModule::SurfaceColor = FLinearColor(0.6, 0.6, 0.6).ToRGBE();

void FDataflowEditorModule::StartupModule()
{
	FDataflowEditorStyle::Get();
	
	// Register type customizations
	if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->RegisterCustomPropertyTypeLayout(FScalarVertexPropertyGroup::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&Dataflow::FScalarVertexPropertyGroupCustomization::MakeInstance));
		PropertyModule->RegisterCustomPropertyTypeLayout(FDataflowFunctionsProperty::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&Dataflow::FFunctionsPropertyCustomization::MakeInstance));
	}

	Dataflow::RenderingCallbacks();
}

void FDataflowEditorModule::ShutdownModule()
{	
	FEditorModeRegistry::Get().UnregisterMode(UDataflowEditorMode::EM_DataflowEditorModeId);

	// Deregister type customizations
	if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(FScalarVertexPropertyGroup::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomPropertyTypeLayout(FDataflowFunctionsProperty::StaticStruct()->GetFName());
	}
}

IMPLEMENT_MODULE(FDataflowEditorModule, DataflowEditor)


#undef LOCTEXT_NAMESPACE
