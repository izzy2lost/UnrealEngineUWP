// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModule.h"

#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEngineRendering.h"
#include "Dataflow/DataflowSNodeFactories.h"

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "DataflowEditor"

const FColor FDataflowEditorModule::SurfaceColor = FLinearColor(0.6, 0.6, 0.6).ToRGBE();

void FDataflowEditorModule::StartupModule()
{
	FDataflowEditorStyle::Get();
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	Dataflow::RenderingCallbacks();
}

void FDataflowEditorModule::ShutdownModule()
{	
	FEditorModeRegistry::Get().UnregisterMode(UDataflowEditorMode::EM_DataflowEditorModeId);
}

IMPLEMENT_MODULE(FDataflowEditorModule, DataflowEditor)


#undef LOCTEXT_NAMESPACE
