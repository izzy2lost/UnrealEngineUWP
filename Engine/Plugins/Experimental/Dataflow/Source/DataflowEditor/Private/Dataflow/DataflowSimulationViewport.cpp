// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationViewport.h"

#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowSimulationViewportClient.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "EditorModeManager.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorScenes.h"
#include "Dataflow/DataflowSimulationPanel.h"

#define LOCTEXT_NAMESPACE "SDataflowSimulationViewport"


SDataflowSimulationViewport::SDataflowSimulationViewport()
{
}

void SDataflowSimulationViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	SAssetEditorViewport::FArguments ParentArgs;
	ParentArgs._EditorViewportClient = InArgs._ViewportClient;
	SAssetEditorViewport::Construct(ParentArgs, InViewportConstructionArgs);
	Client->VisibilityDelegate.BindSP(this, &SDataflowSimulationViewport::IsVisible);

	if(static_cast<FDataflowSimulationScene*>(Client->GetPreviewScene())->CanRunSimulation())
	{
		TSharedPtr<FDataflowSimulationViewportClient> DataflowClient = StaticCastSharedPtr<FDataflowSimulationViewportClient>(Client);
		TWeakPtr<FDataflowSimulationScene> SimulationScene = DataflowClient->GetDataflowEditorToolkit().Pin()->GetSimulationScene();
            
		ViewportOverlay->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.FillWidth(1)
			.Padding(10.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
				.Visibility(EVisibility::Visible)
				.Padding(10.0f, 2.0f)
				[
					SNew(SDataflowSimulationPanel, SimulationScene)
					.ViewInputMin(this, &SDataflowSimulationViewport::GetViewMinInput)
					.ViewInputMax(this, &SDataflowSimulationViewport::GetViewMaxInput)
				]
			]
		];
	}
}

void SDataflowSimulationViewport::OnFocusViewportToSelection()
{
	if(const FDataflowPreviewScene* PreviewScene = static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene()))
	{
		const FBox SceneBoundingBox = PreviewScene->GetBoundingBox();
		Client->FocusViewportOnBox(SceneBoundingBox);
	}
}

UDataflowEditorMode* SDataflowSimulationViewport::GetEdMode() const
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

void SDataflowSimulationViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();
}

bool SDataflowSimulationViewport::IsVisible() const
{
	// Intentionally not calling SEditorViewport::IsVisible because it will return false if our simulation is more than 250ms.
	return ViewportWidget.IsValid();
}

TSharedRef<class SEditorViewport> SDataflowSimulationViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SDataflowSimulationViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SDataflowSimulationViewport::OnFloatingButtonClicked()
{
}

float SDataflowSimulationViewport:: GetViewMinInput() const
{
	return static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene())->GetDataflowContent()->GetSimulationRange()[0];
}

float SDataflowSimulationViewport::GetViewMaxInput() const
{
	return static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene())->GetDataflowContent()->GetSimulationRange()[1];
}


#undef LOCTEXT_NAMESPACE
