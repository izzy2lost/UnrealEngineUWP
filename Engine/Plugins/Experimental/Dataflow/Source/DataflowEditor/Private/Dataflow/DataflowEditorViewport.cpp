// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorViewport.h"

#include "AdvancedPreviewScene.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorViewportClient.h"
#include "EditorModeManager.h"
#include "Dataflow/DataflowEditorViewportToolbar.h"

#define LOCTEXT_NAMESPACE "SDataflowEditorViewport"


SDataflowEditorViewport::SDataflowEditorViewport()
{
}

void SDataflowEditorViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	SAssetEditorViewport::FArguments ParentArgs;
	ParentArgs._EditorViewportClient = InArgs._ViewportClient;
	SAssetEditorViewport::Construct(ParentArgs, InViewportConstructionArgs);
	Client->VisibilityDelegate.BindSP(this, &SDataflowEditorViewport::IsVisible);
}

TSharedPtr<SWidget> SDataflowEditorViewport::MakeViewportToolbar()
{
	return SNew(SDataflowViewportSelectionToolBar, SharedThis(this));
}

void SDataflowEditorViewport::OnFocusViewportToSelection()
{
	if(UDataflowEditorMode* DataflowMode = GetEdMode())
	{
		const FBox SceneBoundingBox = DataflowMode->SceneBoundingBox();
		Client->FocusViewportOnBox(SceneBoundingBox);
	}
}

UDataflowEditorMode* SDataflowEditorViewport::GetEdMode() const
{
	if (const FEditorModeTools* const EditorModeTools = Client->GetModeTools())
	{
		if (UDataflowEditorMode* const DataflowEdMode = Cast<UDataflowEditorMode>(EditorModeTools->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			return DataflowEdMode;
		}
	}
	return nullptr;
}

void SDataflowEditorViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();
}

bool SDataflowEditorViewport::IsVisible() const
{
	// Intentionally not calling SEditorViewport::IsVisible because it will return false if our simulation is more than 250ms.
	return ViewportWidget.IsValid();
}

TSharedRef<class SEditorViewport> SDataflowEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SDataflowEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SDataflowEditorViewport::OnFloatingButtonClicked()
{
}

#undef LOCTEXT_NAMESPACE
