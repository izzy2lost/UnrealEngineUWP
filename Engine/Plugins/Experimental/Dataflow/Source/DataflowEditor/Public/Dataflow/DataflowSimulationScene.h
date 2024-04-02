// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowEditorPreviewSceneBase.h"

class UDataflowEditor;


/**
 * Dataflow simulation scene holding all the dataflow content components
 */
class DATAFLOWEDITOR_API FDataflowSimulationScene : public FDataflowPreviewSceneBase
{
public:

	FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* Editor);
	virtual ~FDataflowSimulationScene();

	/** Tick data flow scene */
	virtual void TickDataflowScene(const float DeltaSeconds) override;
	
	/** Check if the preview scene can run simulation */
	virtual bool CanRunSimulation() const { return true; }

	void ResetSimulationScene() {}
	bool HasRenderableGeometry() { return true; }

};


