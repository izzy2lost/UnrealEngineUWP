// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowConstructionViewportClient.h"

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



FDataflowConstructionViewportClient::FDataflowConstructionViewportClient(FEditorModeTools* InModeTools,
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

void FDataflowConstructionViewportClient::SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
{
	DataflowEditorToolkitPtr = InDataflowEditorToolkitPtr;
}

void FDataflowConstructionViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}

//const UInputBehaviorSet* FDataflowConstructionViewportClient::GetInputBehaviors() const
//{
//	return BehaviorSet;
//}

void FDataflowConstructionViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PreviewScene)
	{
		PreviewScene->TickDataflowScene(DeltaSeconds);
	}
}

USelection* FDataflowConstructionViewportClient::GetSelectedComponents() 
{ 
	return ModeTools->GetSelectedComponents(); 
}


void FDataflowConstructionViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	auto EnableToolForSelectedNode = [&](USelection* SelectedComponents)
	{
		if (TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin())
		{
			if (PreviewScene && PreviewScene->GetDataflowModeManager())
			{
				if (UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(PreviewScene->GetDataflowModeManager()->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
				{
					if (TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowEditorToolkit->GetDataflowGraphEditor())
					{
						if (UEdGraphNode* SelectedNode = GraphEditor->GetSingleSelectedNode())
						{
							if (SelectedComponents && SelectedComponents->Num() == 1)
							{
								if (UDataflowEditorCollectionComponent* CollectionComponent =
									Cast< UDataflowEditorCollectionComponent>(SelectedComponents->GetSelectedObject(0)))
								{
									if (CollectionComponent->Node == SelectedNode)
									{
										// Start the corresponding tool
										DataflowMode->StartToolForSelectedNode(SelectedNode);

										return CollectionComponent;
									}
								}
							}
						}
					}
				}
			}
		}
		return (UDataflowEditorCollectionComponent*)nullptr;
	};

	auto UpdateSelectedComponentInViewport = [&](USelection* SelectedComponents)
	{
		TArray<UPrimitiveComponent*> PreviouslySelectedComponents;
		SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(PreviouslySelectedComponents);

		SelectedComponents->Modify();
		SelectedComponents->BeginBatchSelectOperation();

		SelectedComponents->DeselectAll();

		if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
		{
			const HActor* ActorProxy = static_cast<HActor*>(HitProxy);
			if (ActorProxy && ActorProxy->PrimComponent && ActorProxy->Actor)
			{
				UPrimitiveComponent* Component = const_cast<UPrimitiveComponent*>(ActorProxy->PrimComponent.Get());
				SelectedComponents->Select(Component);
				Component->PushSelectionToProxy();
			}
		}

		SelectedComponents->EndBatchSelectOperation();

		for (UPrimitiveComponent* const Component : PreviouslySelectedComponents)
		{
			Component->PushSelectionToProxy();
		}

	};

	auto SelectSingleNodeInGraph = [&](TObjectPtr<const UDataflowEdNode> Node)
	{
		if (TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin())
		{
			if (TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowEditorToolkit->GetDataflowGraphEditor())
			{
				GraphEditor->GetGraphPanel()->SelectionManager.SelectSingleNode((UObject*)Node.Get());
			}
		}
	};
	
	auto IsInteractiveToolActive = [&]()
	{
		if (UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(
			PreviewScene->GetDataflowModeManager()->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			if (UEditorInteractiveToolsContext* const ToolsContext = DataflowMode->GetInteractiveToolsContext())
			{
				return ToolsContext->HasActiveTool();
			}
		}
		return false;
	};

	auto IsolateComponent = [&](UDataflowEditorCollectionComponent* SelectedComponent)
	{
		if( FDataflowConstructionScene* Scene = static_cast<FDataflowConstructionScene*>(PreviewScene) )
		{ 
			Scene->SetVisibility(false);
			Scene->SetVisibility(true,SelectedComponent);
		}
	};

	if (!IsInteractiveToolActive())
	{
		if (USelection* SelectedComponents = ModeTools->GetSelectedComponents())
		{
			UpdateSelectedComponentInViewport(SelectedComponents);

			if (bool bIsAltKeyDown = Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt))
			{
				if (UDataflowEditorCollectionComponent* DataflowComponent
					= SelectedComponents->GetBottom<UDataflowEditorCollectionComponent>())
				{
					SelectSingleNodeInGraph(DataflowComponent->Node);
				}
			}

			EnableToolForSelectedNode(SelectedComponents);
		}
	}
}

void FDataflowConstructionViewportClient::SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InViewMode)
{
	// @todo(Dataflow) : Add support for Sim2D
	//const bool bSwitching2D3D = (ConstructionViewMode == Dataflow::EDataflowPatternVertexType::Sim2D) != (InViewMode == Dataflow::EDataflowPatternVertexType::Sim2D);
	//if (bSwitching2D3D)
	//{
	//	Swap(SavedInactiveViewTransform, ViewTransformPerspective);
	//}

	ConstructionViewMode = Dataflow::EDataflowPatternVertexType::Sim3D;

	
	//if (ConstructionViewMode == EDataflowPatternVertexType::Sim2D)
	//{
	//	for (UInputBehavior* const Behavior : BehaviorsFor2DMode)
	//	{
	//		BehaviorSet->Add(Behavior);
	//	}
	//
	//	const double AbsZ = FMath::Abs(ViewTransformPerspective.GetLocation().Z);
	//	constexpr double CameraFarPlaneWorldZ = -10.0;
	//	constexpr double CameraNearPlaneProportionZ = 0.8;
	//	OverrideFarClipPlane(static_cast<float>(AbsZ - CameraFarPlaneWorldZ));
	//	OverrideNearClipPlane(static_cast<float>(AbsZ * (1.0 - CameraNearPlaneProportionZ)));
	//}
	//else
	//{
	OverrideFarClipPlane(0);
	OverrideNearClipPlane(UE_KINDA_SMALL_NUMBER);
	//}

	//ModeTools->GetInteractiveToolsContext()->InputRouter->DeregisterSource(this);
	//ModeTools->GetInteractiveToolsContext()->InputRouter->RegisterSource(this);

}


Dataflow::EDataflowPatternVertexType FDataflowConstructionViewportClient::GetConstructionViewMode() const
{
	return Dataflow::EDataflowPatternVertexType::Sim3D;// ConstructionViewMode;
}

void FDataflowConstructionViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(BehaviorSet);
}

