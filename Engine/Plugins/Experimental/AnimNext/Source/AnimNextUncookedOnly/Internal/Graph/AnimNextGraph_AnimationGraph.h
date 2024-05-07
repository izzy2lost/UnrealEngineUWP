// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMExportInterface.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "AnimNextGraph_AnimationGraph.generated.h"

class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;
enum class ERigVMGraphNotifType : uint8;

namespace UE::AnimNext::Editor
{
	struct FUtils;
}

/** A single entry in an AnimNext graph asset */
UCLASS(MinimalAPI, Category = "Animation Graphs")
class UAnimNextGraph_AnimationGraph : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMGraphInterface, public IAnimNextRigVMExportInterface
{
	GENERATED_BODY()

	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::Editor::FUtils;	

	// IAnimNextRigVMExportInterface interface
	virtual FAnimNextParamType GetExportType() const override;
	virtual FName GetExportName() const override;
	virtual EAnimNextExportAccessSpecifier GetExportAccessSpecifier() const override;
	virtual void SetExportAccessSpecifier(EAnimNextExportAccessSpecifier InAccessSpecifier, bool bSetupUndoRedo = true) override;

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override;
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) override;
	
	// IAnimNextRigVMGraphInterface interface
	virtual URigVMGraph* GetRigVMGraph() const override;
	virtual URigVMEdGraph* GetEdGraph() const override;
	virtual void SetRigVMGraph(URigVMGraph* InGraph) override;
	virtual void SetEdGraph(URigVMEdGraph* InGraph) override;

protected:
	/** Access specifier - whether the graph's entry point is visible external to this asset */
	UPROPERTY(EditAnywhere, Category = AnimationGraph)
	EAnimNextExportAccessSpecifier Access = EAnimNextExportAccessSpecifier::Private;

	/** The name of the graph */
	UPROPERTY(VisibleAnywhere, Category = AnimationGraph)
	FName GraphName;

	/** RigVM graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> Graph;

	/** Editor graph */
	UPROPERTY()
	TObjectPtr<UAnimNextGraph_EdGraph> EdGraph;
};

// Old deprecated entry
UCLASS()
class UAnimNextGraphEntry : public UAnimNextRigVMAssetEntry
{
	GENERATED_BODY()

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override { return NAME_None; }
};