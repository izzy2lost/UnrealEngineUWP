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
#include "HAL/PlatformApplicationMisc.h"
#include "PreviewScene.h"
#include "Selection.h"

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
			if (TSharedPtr<Dataflow::FContext> Context = EditorContent->DataflowContext)
			{
				if (const UDataflow* Dataflow = EditorContent->DataflowAsset)
				{
					const Dataflow::FTimestamp SystemTimestamp = LatestTimestamp(Dataflow, Context.Get());
					if (SystemTimestamp >= EditorContent->LastModifiedTimestamp)
					{
						PreviewScene->Update();
						EditorContent->LastModifiedTimestamp = LatestTimestamp(EditorContent->DataflowAsset, EditorContent->DataflowContext.Get()).Value + 1;
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

