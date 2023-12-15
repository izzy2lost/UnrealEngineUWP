// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowComponentSelectionState.h"

class FDataflowEditorToolkit;
class ADataflowActor;
class UTransformProxy;
class UCombinedTransformGizmo;
class FTransformGizmoDataBinder;
class FDataflowPreviewScene;

class DATAFLOWEDITOR_API FDataflowEditorViewportClient : public FEditorViewportClient
{
public:
	using Super = FEditorViewportClient;

	FDataflowEditorViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene,
								  const TWeakPtr<SEditorViewport> InEditorViewportWidget = nullptr);

	/** Set the data flow toolkit used to create the client*/
	void SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> DataflowToolkit);

	/** Return the latest timestamp */
	::Dataflow::FTimestamp LatestTimestamp(const UDataflow* Dataflow, const ::Dataflow::FContext* Context);

	// Selection utils
	void SetSelectionMode(FDataflowSelectionState::EMode InState);
	bool CanSetSelectionMode(FDataflowSelectionState::EMode InState);
	bool IsSelectionModeActive(FDataflowSelectionState::EMode InState);
	FDataflowSelectionState::EMode GetSelectionMode() const { return SelectionMode; }
	
	// FEditorViewportClient interface
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void Tick(float DeltaSeconds) override;

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDataflowEditorViewportClient"); }

private:

	/** Toolkit used to create the viewport client */
	TWeakPtr<FDataflowEditorToolkit> DataflowEditorToolkitPtr = nullptr;
	
	/** Last time stamp used for dataflow evaluation on the component */
	::Dataflow::FTimestamp LastModifiedTimestamp = ::Dataflow::FTimestamp::Invalid;

	/** Dataflow preview scene from the toolkit */
	FDataflowPreviewScene* PreviewScene = nullptr;

	/** Selection mode to be used for vertices/faces/objects */
	FDataflowSelectionState::EMode SelectionMode = FDataflowSelectionState::EMode::DSS_Dataflow_None;
};
