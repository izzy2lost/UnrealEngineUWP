// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMGraph.h"
#include "AnimNextGraph_EdGraph.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprint.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimNextGraph_EditorData.generated.h"

class UAnimNextGraph;
enum class ERigVMGraphNotifType : uint8;
class FAnimationAnimNextRuntimeTest_GraphAddDecorator;
class FAnimationAnimNextRuntimeTest_GraphExecute;
class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FGraphEditor;
	class SAnimNextGraphView;
	struct FUtils;
}

enum class EAnimNextGraphLoadType : uint8
{
	PostLoad,
	CheckUserDefinedStructs
};

namespace UE::AnimNext::UncookedOnly
{
// A delegate for subscribing / reacting to graph modifications.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphModified, UAnimNextGraph_EditorData* /* InEditorData */);
}

/**
 * The Schema is used to determine which actions are allowed
 * on a graph. This includes any topological change.
 */
UCLASS()
class UAnimNextGraph_Schema : public URigVMSchema
{
	GENERATED_BODY()
};

// Script-callable editor API hoisted onto UAnimNextGraph
UCLASS()
class ANIMNEXTUNCOOKEDONLY_API UAnimNextGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Adds a graph to an AnimNext Graph asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Graph", meta=(ScriptMethod))
	static UAnimNextGraphEntry* AddGraph(UAnimNextGraph* InGraph, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
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
	friend class UAnimNextGraphEntry;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::Editor::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend class UE::AnimNext::Editor::SAnimNextGraphView;
	friend struct FAnimNextGraphSchemaAction_RigUnit;
	friend struct FAnimNextGraphSchemaAction_DispatchFactory;
	friend class FAnimationAnimNextRuntimeTest_GraphAddDecorator;
	friend class FAnimationAnimNextRuntimeTest_GraphExecute;
	friend class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;

	ANIMNEXTUNCOOKEDONLY_API UAnimNextGraphEntry* AddGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual bool IsEditorOnly() const override { return true; }
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
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
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) override {}
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
	virtual UObject* GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const override;
	virtual URigVMGraph* GetRigVMGraphForEditorObject(UObject* InObject) const override;
	
	// IRigVMGraphFunctionHost interface
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override;
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override;

	UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce);
	bool RemoveEdGraph(URigVMGraph* InModel);

	FRigVMGraphModifiedEvent& GetRigVMGraphModifiedEvent()
	{
		return RigVMGraphModifiedEvent;
	}

	ANIMNEXTUNCOOKEDONLY_API void Initialize(bool bRecompileVM);

protected:
#if WITH_EDITOR
	void RefreshAllModels(EAnimNextGraphLoadType InLoadType);
	void GetAllGraphs(TArray<UEdGraph*>& Graphs) const;
#endif

	void RecompileVM();
	
	void RecompileVMIfRequired();

	void RequestAutoVMRecompilation();

	void IncrementVMRecompileBracket();

	void DecrementVMRecompileBracket();
	
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode);

	void BroadcastModified();

	void ReportError(const TCHAR* InMessage) const;

	void ReconstructAllNodes();

	UPROPERTY()
	TArray<TObjectPtr<UAnimNextGraph_EdGraph>> Graphs;

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

	/** All entries in this graph asset - not saved, either serialized or discovered at load time */
	UPROPERTY(transient)
	TArray<TObjectPtr<UAnimNextGraphEntry>> Entries;
	
	UPROPERTY(transient, DuplicateTransient)
	int32 VMRecompilationBracket = 0;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling = false;
	
	FCompilerResultsLog CompileLog;

	FOnRigVMCompiledEvent RigVMCompiledEvent;
	FRigVMGraphModifiedEvent RigVMGraphModifiedEvent;

	// Delegate to subscribe to modifications to this graph
	UE::AnimNext::UncookedOnly::FOnGraphModified ModifiedDelegate;

	bool bAutoRecompileVM = true;
	bool bErrorsDuringCompilation = false;
	bool bSuspendModelNotificationsForSelf = false;
	bool bSuspendModelNotificationsForOthers = false;
	bool bSuspendPythonMessagesForRigVMClient = false;
	bool bSuspendAllNotifications = false;
	bool bSuspendGraphNotifications = false;
	bool bCompileInDebugMode = false;
};
