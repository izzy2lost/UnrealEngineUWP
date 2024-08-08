// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModule.h"

#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEngineRendering.h"
#include "Dataflow/DataflowFunctionProperty.h"
#include "Dataflow/DataflowFunctionPropertyCustomization.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "Dataflow/ScalarVertexPropertyGroupCustomization.h"
#include "Dataflow/DataflowToolRegistry.h"
#include "DataflowEditorTools/DataflowEditorWeightMapPaintTool.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"

#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "DataflowEditor"

const FColor FDataflowEditorModule::SurfaceColor = FLinearColor(0.6, 0.6, 0.6).ToRGBE();
static const FName ScalarVertexPropertyGroupName = TEXT("ScalarVertexPropertyGroup");
static const FName DataflowFunctionPropertyName = TEXT("DataflowFunctionProperty");

void FDataflowEditorModule::StartupModule()
{
	FDataflowEditorStyle::Get();
	
	// Register type customizations
	if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->RegisterCustomPropertyTypeLayout(ScalarVertexPropertyGroupName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&Dataflow::FScalarVertexPropertyGroupCustomization::MakeInstance));
		PropertyModule->RegisterCustomPropertyTypeLayout(DataflowFunctionPropertyName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&Dataflow::FFunctionPropertyCustomization::MakeInstance));
	}

	Dataflow::RenderingCallbacks();

	Dataflow::FDataflowToolRegistry& ToolRegistry = Dataflow::FDataflowToolRegistry::Get();

	UDataflowEditorWeightMapPaintToolBuilder* const ToolBuilder = NewObject<UDataflowEditorWeightMapPaintToolBuilder>();
	ToolRegistry.AddNodeToToolMapping(FDataflowCollectionAddScalarVertexPropertyNode::StaticType(), ToolBuilder);
}

void FDataflowEditorModule::ShutdownModule()
{	
	FEditorModeRegistry::Get().UnregisterMode(UDataflowEditorMode::EM_DataflowEditorModeId);

	// Deregister type customizations
	if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(ScalarVertexPropertyGroupName);
		PropertyModule->UnregisterCustomPropertyTypeLayout(DataflowFunctionPropertyName);
	}

	Dataflow::FDataflowToolRegistry& ToolRegistry = Dataflow::FDataflowToolRegistry::Get();
	ToolRegistry.RemoveNodeToToolMapping(FDataflowCollectionAddScalarVertexPropertyNode::StaticType());
}

IMPLEMENT_MODULE(FDataflowEditorModule, DataflowEditor)


#undef LOCTEXT_NAMESPACE
