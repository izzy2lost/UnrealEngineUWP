// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowComponentSelectionState.h"
#include "Dataflow/DataflowPatternVertexType.h"
#include "InputBehaviorSet.h"

class FDataflowEditorToolkit;
class ADataflowActor;
class UTransformProxy;
class UCombinedTransformGizmo;
class FTransformGizmoDataBinder;
class FDataflowPreviewScene;
class UInputBehaviorSet;

class DATAFLOWEDITOR_API FDataflowEditorViewportClient : public FEditorViewportClient //, public IInputBehaviorSource
{
//@todo(brice) : Add BehaviorUI support

public:
	using Super = FEditorViewportClient;

	FDataflowEditorViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene,
								  const TWeakPtr<SEditorViewport> InEditorViewportWidget = nullptr);

	void SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InViewMode);
	Dataflow::EDataflowPatternVertexType GetConstructionViewMode() const;

	// IInputBehaviorSource
	// virtual const UInputBehaviorSet* GetInputBehaviors() const override;

	/** Set the data flow toolkit used to create the client*/
	void SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> DataflowToolkit);
	
	void SetToolCommandList(TWeakPtr<FUICommandList> ToolCommandList);

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDataflowEditorViewportClient"); }

private:

	virtual void Tick(float DeltaSeconds) override;

	/** Toolkit used to create the viewport client */
	TWeakPtr<FDataflowEditorToolkit> DataflowEditorToolkitPtr = nullptr;

	/** Dataflow preview scene from the toolkit */
	FDataflowPreviewScene* PreviewScene = nullptr;

	// @todo(brice) : Is this needed?
	TWeakPtr<FUICommandList> ToolCommandList;

	Dataflow::EDataflowPatternVertexType ConstructionViewMode = Dataflow::EDataflowPatternVertexType::Sim3D;

	TObjectPtr<UInputBehaviorSet> BehaviorSet;

};
