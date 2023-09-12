// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorMode.h"

#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/AssetDefinition_DataflowAsset.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "DataflowEditor"

//#define BOX_BRUSH(StyleSet, RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
//#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

void FDataflowEditorModule::StartupModule()
{
	FDataflowEditorStyle::Get();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	DataflowSNodeFactory = MakeShareable(new FDataflowSNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(DataflowSNodeFactory);
}

void FDataflowEditorModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(DataflowSNodeFactory);
	}
	FEditorModeRegistry::Get().UnregisterMode(UDataflowEditorMode::EM_DataflowEditorModeId);

}

IMPLEMENT_MODULE(FDataflowEditorModule, DataflowEditor)


#undef LOCTEXT_NAMESPACE
