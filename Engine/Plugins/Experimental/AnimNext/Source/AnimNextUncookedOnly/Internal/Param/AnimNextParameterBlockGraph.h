// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.h"
#include "IAnimNextParameterBlockGraphInterface.h"
#include "AnimNextParameterBlockGraph.generated.h"

class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;
class URigVMGraph;

UCLASS()
class UAnimNextParameterBlockGraph : public UAnimNextParameterBlockEntry, public IAnimNextParameterBlockGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;

	// UAnimNextParameterBlockEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	// IAnimNextParameterBlockGraphInterface interface
	virtual URigVMGraph* GetGraph() const override { return Graph; }
	virtual FName GetGraphName() const override { return GraphName; }
	virtual void SetGraphName(FName InName, bool bSetupUndoRedo = true) override;

	/** The name of the graph */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName GraphName;

	/** Graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> Graph;
};