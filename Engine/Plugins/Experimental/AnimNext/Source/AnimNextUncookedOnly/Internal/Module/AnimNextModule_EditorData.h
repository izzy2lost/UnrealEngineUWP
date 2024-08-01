// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Module/AnimNextModule_Controller.h"
#include "RigVMModel/RigVMGraph.h"
#include "AnimNextEdGraph.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "AnimNextExecuteContext.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimNextModule_EditorData.generated.h"

class UAnimNextModule;
class UAnimNextModule_FunctionGraph;
enum class ERigVMGraphNotifType : uint8;
class FAnimationAnimNextRuntimeTest_GraphAddTrait;
class FAnimationAnimNextRuntimeTest_GraphExecute;
class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FModuleEditor;
	class SAnimNextGraphView;
	struct FUtils;
}

// Script-callable editor API hoisted onto UAnimNextModule
UCLASS()
class ANIMNEXTUNCOOKEDONLY_API UAnimNextModuleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Adds an animation graph to an AnimNext Module asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Module", meta=(ScriptMethod))
	static UAnimNextModule_AnimationGraph* AddAnimationGraph(UAnimNextModule* InModule, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a parameter to an AnimNext Module asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Module", meta=(ScriptMethod))
	static UAnimNextModule_Parameter* AddParameter(UAnimNextModule* InModule, FName InName, EPropertyBagPropertyType InValueType, EPropertyBagContainerType InContainerType = EPropertyBagContainerType::None, const UObject* InValueTypeObject = nullptr, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds an event graph to an AnimNext Module asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Module", meta=(ScriptMethod))
	static UAnimNextModule_EventGraph* AddEventGraph(UAnimNextModule* InModule, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
};

/** Editor data for AnimNext modules */
UCLASS(MinimalAPI)
class UAnimNextModule_EditorData : public UAnimNextRigVMAssetEditorData
{
	GENERATED_BODY()

	friend class UAnimNextModuleFactory;
	friend class UAnimNextEdGraph;
	friend class UAnimNextGraphEntry;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::Editor::FUtils;
	friend class UE::AnimNext::Editor::FModuleEditor;
	friend class UE::AnimNext::Editor::SAnimNextGraphView;
	friend struct FAnimNextGraphSchemaAction_RigUnit;
	friend struct FAnimNextGraphSchemaAction_DispatchFactory;
	friend class FAnimationAnimNextEditorTest_GraphAddTrait;
	friend class FAnimationAnimNextEditorTest_GraphTraitOperations;
	friend class FAnimationAnimNextRuntimeTest_GraphExecute;
	friend class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;
	
public:
	/** Adds an animation graph to this asset */
	ANIMNEXTUNCOOKEDONLY_API UAnimNextModule_AnimationGraph* AddAnimationGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a parameter to this asset */
	ANIMNEXTUNCOOKEDONLY_API UAnimNextModule_Parameter* AddParameter(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds an event graph to this asset */
	ANIMNEXTUNCOOKEDONLY_API UAnimNextModule_EventGraph* AddEventGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

private:
	// UObject interface
	virtual void PostLoad() override;

	// IRigVMClientHost interface
	virtual void RecompileVM() override;

	// UAnimNextRigVMAssetEditorData interface
	virtual TSubclassOf<URigVMController> GetControllerClass() const override { return UAnimNextModule_Controller::StaticClass(); }
	virtual UScriptStruct* GetExecuteContextStruct() const override { return FAnimNextExecuteContext::StaticStruct(); }
	virtual UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce) override;
	virtual bool RemoveEdGraph(URigVMGraph* InModel) override;
	virtual void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bForce) override;
	virtual void RemoveEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bNotify) override;
	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) override;
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UAnimNextEdGraph>> Graphs_DEPRECATED;
};
