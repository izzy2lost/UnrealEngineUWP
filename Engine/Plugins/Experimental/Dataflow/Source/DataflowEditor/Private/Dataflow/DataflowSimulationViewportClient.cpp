// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowSimulationViewportClient.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowEditorScenes.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "GraphEditor.h"
#include "PreviewScene.h"
#include "Selection.h"
#include "SGraphPanel.h"
#include "SNodePanel.h"



FDataflowSimulationViewportClient::FDataflowSimulationViewportClient(FEditorModeTools* InModeTools,
                                                             FPreviewScene* InPreviewScene,  const bool bCouldTickScene,
                                                             const TWeakPtr<SEditorViewport> InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.EnableAdvancedFeatures();

	PreviewScene = static_cast<FDataflowPreviewScene*>(InPreviewScene);
	bEnableSceneTicking = bCouldTickScene;
}

void FDataflowSimulationViewportClient::SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
{
	DataflowEditorToolkitPtr = InDataflowEditorToolkitPtr;
}

void FDataflowSimulationViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}

//const UInputBehaviorSet* FDataflowSimulationViewportClient::GetInputBehaviors() const
//{
//	return BehaviorSet;
//}

void FDataflowSimulationViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PreviewScene)
	{
		PreviewScene->TickDataflowScene(DeltaSeconds);
	}
}

void FDataflowSimulationViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
}


void FDataflowSimulationViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(BehaviorSet);
}

