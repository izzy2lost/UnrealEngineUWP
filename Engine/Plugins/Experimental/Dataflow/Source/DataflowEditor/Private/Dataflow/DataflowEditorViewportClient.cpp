// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewportClient.h"

#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorContent.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "Dataflow/DataflowXml.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "HAL/PlatformApplicationMisc.h"
#include "PreviewScene.h"
#include "Selection.h"
#include "InputBehaviorSet.h"

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

void FDataflowEditorViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}

//const UInputBehaviorSet* FDataflowEditorViewportClient::GetInputBehaviors() const
//{
//	return BehaviorSet;
//}

Dataflow::FTimestamp LatestTimestamp(const UDataflow* Dataflow, const ::Dataflow::FContext* Context)
{
	if (Dataflow && Context)
	{
		return FMath::Max(Dataflow->GetRenderingTimestamp().Value, Context->GetTimestamp().Value);
	}
	return ::Dataflow::FTimestamp::Invalid;
}

void FDataflowEditorViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin();

	if (PreviewScene)
	{
		if (TObjectPtr<UDataflowEditorContent> EditorContent = PreviewScene->GetDataflowEditorContent())
		{
			if (TSharedPtr<Dataflow::FContext> Context = EditorContent->GetDataflowContext())
			{
				if (const UDataflow* Dataflow = EditorContent->GetDataflowAsset())
				{
					const Dataflow::FTimestamp SystemTimestamp = LatestTimestamp(Dataflow, Context.Get());
					if (SystemTimestamp >= EditorContent->GetLastModifiedTimestamp())
					{
						EditorContent->SetLastModifiedTimestamp(LatestTimestamp(EditorContent->GetDataflowAsset(), EditorContent->GetDataflowContext().Get()).Value + 1);
					}
				}

				if (EditorContent->IsDirty())
				{
					PreviewScene->Update();
				}
			}
		}
	}
}

void FDataflowEditorViewportClient::SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InViewMode)
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


Dataflow::EDataflowPatternVertexType FDataflowEditorViewportClient::GetConstructionViewMode() const
{
	return Dataflow::EDataflowPatternVertexType::Sim3D;// ConstructionViewMode;
}

void FDataflowEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(BehaviorSet);
}

