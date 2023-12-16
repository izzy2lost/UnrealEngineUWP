// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewportClient.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowXml.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Selection.h"
#include "HAL/PlatformApplicationMisc.h"
#include "PreviewScene.h"
#include "EditorModeManager.h"

FDataflowEditorViewportClient::FDataflowEditorViewportClient(FEditorModeTools* InModeTools,
                                                             FPreviewScene* InPreviewScene,
                                                             const TWeakPtr<SEditorViewport> InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.EnableAdvancedFeatures();

	PreviewScene = static_cast<FDataflowPreviewScene*>(InPreviewScene);
}

void FDataflowEditorViewportClient::SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
{
	DataflowEditorToolkitPtr = InDataflowEditorToolkitPtr;
}

void FDataflowEditorViewportClient::SetSelectionMode(FDataflowSelectionState::EMode InState)
{
	FDataflowSelectionState State = PreviewScene->GetDataflowComponent()->GetSelectionState();

	if (SelectionMode == InState)
	{
		SelectionMode = FDataflowSelectionState::EMode::DSS_Dataflow_None;
	}
	else
	{
		SelectionMode = InState;
	}

	State.Mode = SelectionMode;
	PreviewScene->GetDataflowComponent()->SetSelectionState(State);

	if (SelectionMode == FDataflowSelectionState::EMode::DSS_Dataflow_None)
	{
		if (!PreviewScene->GetDataflowComponent()->GetSelectionState().IsEmpty())
		{
			PreviewScene->GetDataflowComponent()->SetSelectionState(FDataflowSelectionState(SelectionMode));
		}
	}
}
bool FDataflowEditorViewportClient::CanSetSelectionMode(FDataflowSelectionState::EMode InState)
{
	TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin();
	if (DataflowEditorToolkitPtr.IsValid())
	{
		if (TObjectPtr<UDataflowEditorContent> EditorContent = PreviewScene->GetDataflowEditorContent())
		{
			if (const UDataflow* Dataflow = EditorContent->DataflowAsset)
			{
				if (Dataflow->GetRenderTargets().Num())
				{
					if (InState == FDataflowSelectionState::EMode::DSS_Dataflow_Object)
					{
						return true;
					}

					if (InState == FDataflowSelectionState::EMode::DSS_Dataflow_Vertex
						&& !PreviewScene->GetDataflowComponent()->GetSelectionState().Nodes.IsEmpty())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FDataflowEditorViewportClient::IsSelectionModeActive(FDataflowSelectionState::EMode InState)
{
	return SelectionMode == InState;
}

::Dataflow::FTimestamp FDataflowEditorViewportClient::LatestTimestamp(const UDataflow* Dataflow, const ::Dataflow::FContext* Context)
{
	if (Dataflow && Context)
	{
		return FMath::Max(Dataflow->GetRenderingTimestamp().Value, Context->GetTimestamp().Value);
	}
	return ::Dataflow::FTimestamp::Invalid;
}

bool FDataflowEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = false;

	FInputEventState InputState(EventArgs.Viewport, EventArgs.Key, EventArgs.Event);
	if (InputState.IsCtrlButtonPressed() && EventArgs.Key == EKeys::C)
	{
		if (PreviewScene->GetDataflowComponent()->GetSelectionState().Vertices.Num())
		{
			FString XmlBuffer = FDataflowXmlWrite()
				.Begin()
				.MakeVertexSelectionBlock(PreviewScene->GetDataflowComponent()->GetSelectionState().Vertices)
				.End()
				.ToString();
			FPlatformApplicationMisc::ClipboardCopy(*XmlBuffer);
		}
	}


	if (!bHandled)
	{
		bHandled = FEditorViewportClient::InputKey(EventArgs);
	}

	return bHandled;
}

void FDataflowEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
	if (PreviewScene->GetDataflowComponent())
	{
		const bool bIsShiftKeyDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);
		const bool bIsCtrltKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);

		USelection* SelectedComponents = ModeTools->GetSelectedComponents();

		TArray<UPrimitiveComponent*> PreviouslySelectedComponents;
		SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(PreviouslySelectedComponents);

		SelectedComponents->Modify();
		SelectedComponents->BeginBatchSelectOperation();

		SelectedComponents->DeselectAll();

		if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
		{
			const HActor* const ActorProxy = static_cast<HActor*>(HitProxy);
			if (ActorProxy && ActorProxy->Actor)
			{
				const AActor* const Actor = ActorProxy->Actor;
			
				Actor->ForEachComponent<UPrimitiveComponent>(true, [&](UPrimitiveComponent* Component)
				{
					SelectedComponents->Select(Component);
					Component->PushSelectionToProxy();
				});
			}
		}

		SelectedComponents->EndBatchSelectOperation();

		for (UPrimitiveComponent* const Component : PreviouslySelectedComponents)
		{
			Component->PushSelectionToProxy();
		}

		FDataflowSelectionState SelectionState = PreviewScene->GetDataflowComponent()->GetSelectionState();
		FDataflowSelectionState PreState = SelectionState;

		if (SelectionMode == FDataflowSelectionState::EMode::DSS_Dataflow_Object)
		{
			if (HitProxy && HitProxy->IsA(HDataflowNode::StaticGetType()))
			{
				HDataflowNode* DataflowNode = (HDataflowNode*)(HitProxy);
				FDataflowSelectionState::ObjectID ID(DataflowNode->NodeName, DataflowNode->GeometryIndex);

				if (bIsShiftKeyDown)
				{
					if (!SelectionState.Nodes.Contains(ID))
					{
						SelectionState.Nodes.AddUnique(ID);
					}
				}
				else if (bIsCtrltKeyDown)
				{
					if (SelectionState.Nodes.Contains(ID))
					{
						SelectionState.Nodes.Remove(ID);
					}
				}
				else
				{
					SelectionState.Nodes.Empty();
					SelectionState.Nodes.AddUnique(ID);
				}
			}
			else if (!bIsShiftKeyDown && !bIsCtrltKeyDown)
			{
				SelectionState.Nodes.Empty();
			}
		}

		if (SelectionMode == FDataflowSelectionState::EMode::DSS_Dataflow_Vertex)
		{
			if (HitProxy && HitProxy->IsA(HDataflowVertex::StaticGetType()))
			{
				HDataflowVertex* DataflowVertex = (HDataflowVertex*)(HitProxy);
				int32 ID = DataflowVertex->SectionIndex;
				if (bIsShiftKeyDown)
				{
					if (!SelectionState.Vertices.Contains(ID))
					{
						SelectionState.Vertices.AddUnique(ID);
					}
				}
				else if (bIsCtrltKeyDown)
				{
					if (SelectionState.Vertices.Contains(ID))
					{
						SelectionState.Vertices.Remove(ID);
					}
				}
				else
				{
					SelectionState.Vertices.Empty();
					SelectionState.Vertices.AddUnique(ID);
				}
			}
			else if (!bIsShiftKeyDown && !bIsCtrltKeyDown)
			{
				SelectionState.Vertices.Empty();
			}
		}

		if (PreState != SelectionState)
		{
			PreviewScene->GetDataflowComponent()->SetSelectionState(SelectionState);
		}
	}
}

void FDataflowEditorViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin();

	if (PreviewScene->GetDataflowComponent())
	{
		if (TObjectPtr<UDataflowEditorContent> EditorContent = PreviewScene->GetDataflowEditorContent())
		{
			if (TSharedPtr<Dataflow::FContext> Context = EditorContent->DataflowContext)
			{
				if (const UDataflow* Dataflow = EditorContent->DataflowAsset)
				{
					const Dataflow::FTimestamp SystemTimestamp = LatestTimestamp(Dataflow, Context.Get());
					if (SystemTimestamp >= LastModifiedTimestamp)
					{
						PreviewScene->UpdateDataflowComponent();
						LastModifiedTimestamp = LatestTimestamp(Dataflow, Context.Get()).Value + 1;
					}
				}
			}
		}
	}
}

void FDataflowEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

