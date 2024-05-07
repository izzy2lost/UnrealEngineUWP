// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMExportInterface.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "IAnimNextRigVMParameterInterface.h"
#include "AnimNextGraph_EventGraph.generated.h"

class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;

namespace UE::AnimNext::Tests
{
	class FEditor_Parameters;
}

UCLASS(MinimalAPI, Category = "Event Graphs")
class UAnimNextGraph_EventGraph : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextGraph_EditorData;
	friend class UE::AnimNext::Tests::FEditor_Parameters;

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override { return GraphName; }
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	// IAnimNextRigVMGraphInterface interface
	virtual URigVMGraph* GetRigVMGraph() const override;
	virtual URigVMEdGraph* GetEdGraph() const override;
	virtual void SetRigVMGraph(URigVMGraph* InGraph) override;
	virtual void SetEdGraph(URigVMEdGraph* InGraph) override;

	/** The name of the graph */
	UPROPERTY(VisibleAnywhere, Category = "Event Graph")
	FName GraphName;

	/** Graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> Graph;

	/** Graph */
	UPROPERTY()
	TObjectPtr<UAnimNextGraph_EdGraph> EdGraph;
};

// Old deprecated entry
UCLASS()
class UAnimNextParameterBlockGraph : public UAnimNextRigVMAssetEntry
{
	GENERATED_BODY()

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override { return NAME_None; }
};