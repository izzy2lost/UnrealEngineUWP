// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorMode.h"

#include "DataflowEditorMode.generated.h"


/**
 * The dataflow editor mode is the mode used in the cloth asset editor. It holds most of the inter-tool state.
 * We put things in a mode instead of directly into the asset editor in case we want to someday use the mode
 * in multiple asset editors.
 */
UCLASS(Transient)
class DATAFLOWEDITOR_API UDataflowEditorMode final : public UBaseCharacterFXEditorMode
{
	GENERATED_BODY()

public:

	const static FEditorModeID EM_DataflowEditorModeId;

	UDataflowEditorMode();

private:
	// UEdMode
	virtual void CreateToolkit() override;

	// UBaseCharacterFXEditorMode
	virtual void AddToolTargetFactories() override {}
	virtual void RegisterTools() override {}
	virtual void CreateToolTargets(const TArray<TObjectPtr<UObject>>& AssetsIn) override {}

};

