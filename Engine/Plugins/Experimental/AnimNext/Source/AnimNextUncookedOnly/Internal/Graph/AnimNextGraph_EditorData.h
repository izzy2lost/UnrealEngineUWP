// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMGraph.h"
#include "AnimNextGraph_EdGraph.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprint.h"
#include "AnimNextGraph_EditorData.generated.h"

class UAnimNextGraph;
enum class ERigVMGraphNotifType : uint8;
namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FGraphEditor;
}

enum class EAnimNextGraphLoadType : uint8
{
	PostLoad,
	CheckUserDefinedStructs
};

/**
 * The Schema is used to determine which actions are allowed
 * on a graph. This includes any topological change.
 */
UCLASS()
class UAnimNextGraph_Schema : public URigVMSchema
{
	GENERATED_BODY()
};

/**
  * Implements a RigVM client host and RigVM graph function store host
  * 
  * A RigVM client holds a graph schema, multiple graph models and controllers, as well as an action stack, undo/redo
  * information, and a function library.
  * 
  * A RigVM function store holds various RigVM functions that can be called and linked against. Functions are created
  * in the editor UI under the 'My Blueprint' tab.
  */
UCLASS(MinimalAPI)
class UAnimNextGraph_EditorData : public UObject, public IRigVMClientHost, public IRigVMGraphFunctionHost
{
	GENERATED_BODY()

public:
	UAnimNextGraph_EditorData(const FObjectInitializer& ObjectInitializer);
	
	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EdGraph;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend struct FAnimNextGraphSchemaAction_RigUnit;
	friend struct FAnimNextGraphSchemaAction_DispatchFactory;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual bool IsEditorOnly() const override { return true; }
#if WITH_EDITOR
	void HandlePackageDone(const FEndLoadPackageContext& Context);
	void HandlePackageDone();
#endif // WITH_EDITOR

	// IRigVMClientHost interface
	virtual FRigVMClient* GetRigVMClient() override;
	virtual const FRigVMClient* GetRigVMClient() const override;
	virtual IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() override;
	virtual const IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() const override;
	virtual void HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath) override {}
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) override {}
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
	virtual UObject* GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const override;

	// IRigVMGraphFunctionHost interface
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override;
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override;

	ANIMNEXTUNCOOKEDONLY_API void Initialize(bool bRecompileVM);
	ANIMNEXTUNCOOKEDONLY_API UAnimNextGraph_EdGraph* GetRootGraph() const;

protected:
#if WITH_EDITOR
	void RefreshAllModels(EAnimNextGraphLoadType InLoadType);
	ANIMNEXTUNCOOKEDONLY_API void RebuildEdGraphFromModel();
	void GetAllGraphs(TArray<UEdGraph*>& Graphs) const;
#endif

	void RecompileVM();
	
	void RecompileVMIfRequired();

	void RequestAutoVMRecompilation();

	void IncrementVMRecompileBracket();

	void DecrementVMRecompileBracket();
	
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	ANIMNEXTUNCOOKEDONLY_API URigVMGraph* GetVMGraphForEdGraph(const UEdGraph* InGraph) const;

	void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode);

	bool IsNodeExecConnected(const UEdGraphNode* Node) const;

	UPROPERTY()
	TObjectPtr<UAnimNextGraph_EdGraph> RootGraph;

	UPROPERTY()
	TObjectPtr<UAnimNextGraph_EdGraph> EntryPointGraph;

	UPROPERTY()
	TObjectPtr<UAnimNextGraph_EdGraph> FunctionLibraryEdGraph;

	UPROPERTY()
	FRigVMClient RigVMClient;

	UPROPERTY()
	FRigVMGraphFunctionStore GraphFunctionStore;

	UPROPERTY()
	TObjectPtr<URigVMLibraryNode> EntryPoint;

	UPROPERTY(transient)
	TMap<TObjectPtr<URigVMGraph>, TObjectPtr<URigVMController>> Controllers;

	UPROPERTY(EditAnywhere, Category = "User Interface")
	FRigVMEdGraphDisplaySettings RigGraphDisplaySettings;

	UPROPERTY(EditAnywhere, Category = "VM")
	FRigVMRuntimeSettings VMRuntimeSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VM", meta = (AllowPrivateAccess = "true"))
	FRigVMCompileSettings VMCompileSettings;

	UPROPERTY(transient, DuplicateTransient)
	TMap<FString, FRigVMOperand> PinToOperandMap;

	UPROPERTY(transient, DuplicateTransient)
	int32 VMRecompilationBracket = 0;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling = false;
	
	FCompilerResultsLog CompileLog;

	FOnRigVMCompiledEvent VMCompiledEvent;
	FRigVMGraphModifiedEvent ModifiedEvent;

	bool bAutoRecompileVM = true;
	bool bErrorsDuringCompilation = false;
	bool bSuspendModelNotificationsForSelf = false;
	bool bSuspendModelNotificationsForOthers = false;
	bool bSuspendAllNotifications = false;
	bool bCompileInDebugMode = false;
};
