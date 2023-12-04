// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#pragma once

#include "CoreMinimal.h"
#include "SSchematicGraphPanel.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

/** Called back when a viewport is created */
DECLARE_DELEGATE_OneParam(FOnSchematicViewportCreated, const TSharedRef<class SSchematicGraphPanel>&);

/** Arguments used to create a persona viewport tab */
struct ANIMATIONWIDGETS_API FSchematicViewportArgs
{
	FSchematicViewportArgs()
	{}

	/** The model which contains the graph to display */
	FSchematicGraph* SchematicGraph;
	
	/** Delegate fired when the viewport is created */
	FOnSchematicViewportCreated OnViewportCreated;
};


struct ANIMATIONWIDGETS_API FSchematicViewportTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FSchematicViewportTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const FSchematicViewportArgs& InArgs);
	
	virtual FTabSpawnerEntry& RegisterTabSpawner(TSharedRef<FTabManager> TabManager, const FApplicationMode* CurrentApplicationMode) const;
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	FSchematicGraph* SchematicGraph;
	FOnSchematicViewportCreated OnViewportCreated;
};
#endif