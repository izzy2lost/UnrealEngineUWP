// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowConstructionViewportClient.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowConstructionScene.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowRenderingViewMode.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "GraphEditor.h"
#include "PreviewScene.h"
#include "Selection.h"
#include "SGraphPanel.h"
#include "SNodePanel.h"

#define LOCTEXT_NAMESPACE "DataflowConstructionViewportClient"

FDataflowConstructionViewportClient::FDataflowConstructionViewportClient(FEditorModeTools* InModeTools,
                                                             FPreviewScene* InPreviewScene,  const bool bCouldTickScene,
                                                             const TWeakPtr<SEditorViewport> InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.EnableAdvancedFeatures();

	PreviewScene = static_cast<FDataflowPreviewSceneBase*>(InPreviewScene);
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

	TArray<UPrimitiveComponent*> CurrentlySelectedComponents;
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

			SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(CurrentlySelectedComponents);
		}
	}
	OnSelectionChangedMulticast.Broadcast(CurrentlySelectedComponents);
}

void FDataflowConstructionViewportClient::SetConstructionViewMode(const Dataflow::IDataflowConstructionViewMode* InViewMode)
{
	checkf(InViewMode, TEXT("SetConstructionViewMode received null IDataflowConstructionViewMode pointer"));

	if (ConstructionViewMode)
	{
		SavedInactiveViewTransforms.FindOrAdd(ConstructionViewMode->GetName()) = GetViewTransform();
	}

	ConstructionViewMode = InViewMode;

	SetViewportType((ELevelViewportType)ConstructionViewMode->GetViewportType());

	if (const FViewportCameraTransform* const FoundPreviousTransform = SavedInactiveViewTransforms.Find(InViewMode->GetName()))
	{
		if (ConstructionViewMode->IsPerspective())
		{
			ViewTransformPerspective = *FoundPreviousTransform;
		}
		else
		{
			ViewTransformOrthographic = *FoundPreviousTransform;
		}
	}
	else
	{
		// TODO: Default view transform
	}

	bDrawAxes = ConstructionViewMode->IsPerspective();
	Invalidate();
}


void FDataflowConstructionViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(BehaviorSet);
}

#undef LOCTEXT_NAMESPACE 
