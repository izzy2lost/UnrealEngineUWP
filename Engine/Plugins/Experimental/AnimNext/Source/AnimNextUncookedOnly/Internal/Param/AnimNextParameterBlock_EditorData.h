// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMGraph.h"
#include "AnimNextParameterBlock_EdGraph.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprint.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Param/ParamType.h"
#include "AnimNextParameterBlock_EditorData.generated.h"

class UAnimNextParameterBlock;
class UAnimNextParameterBlockEntry;
class UAnimNextParameterBlockParameter;
class UAnimNextParameterBlock_Controller;
enum class ERigVMGraphNotifType : uint8;
class FAnimationAnimNextParametersEditorTest_Block;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FUtilsPrivate;
}

namespace UE::AnimNext::Editor
{
	struct FUtils;
	class FParameterBlockEditor;
	class SParameterBlockView;
	class SParameterPicker;
	class FParameterBlockTabSummoner;
	class SParameterBlockViewRow;
	class FParameterBlockParameterCustomization;
}

namespace UE::AnimNext::UncookedOnly
{
	// A delegate for subscribing / reacting to parameter block modifications.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnParameterBlockModified, UAnimNextParameterBlock_EditorData* /* InEditorData */);
}

enum class UE_DEPRECATED(5.4, "Please use ERigVMLoadType") EAnimNextParameterLoadType : uint8
{
	PostLoad,
	CheckUserDefinedStructs
};

UCLASS()
class UAnimNextParameterBlockLibrary_Schema : public URigVMSchema
{
	GENERATED_BODY()
};

// Script-callable editor API hoisted onto UAnimNextParameterBlock
UCLASS()
class ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockParameter* AddParameter(UAnimNextParameterBlock* InBlock, FName InName, EPropertyBagPropertyType InValueType, EPropertyBagContainerType InContainerType = EPropertyBagContainerType::None, const UObject* InValueTypeObject = nullptr, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockGraph* AddGraph(UAnimNextParameterBlock* InBlock, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
	
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockBinding* AddBinding(UAnimNextParameterBlock* InBlock, FName InName, EPropertyBagPropertyType InValueType, EPropertyBagContainerType InContainerType = EPropertyBagContainerType::None, const UObject* InValueTypeObject = nullptr, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockBindingReference* AddBindingReference(UAnimNextParameterBlock* InBlock, FName InName, UAnimNextParameterBlock* InReferencedBlock, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static bool RemoveAllBindings(UAnimNextParameterBlock* InBlock, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static bool RemoveEntry(UAnimNextParameterBlock* InBlock, UAnimNextParameterBlockEntry* InEntry, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static bool RemoveEntries(UAnimNextParameterBlock* InBlock, const TArray<UAnimNextParameterBlockEntry*>& InEntries, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockEntry* FindBinding(UAnimNextParameterBlock* InBlock, FName InName);
};

UCLASS(MinimalAPI)
class UAnimNextParameterBlock_EditorData : public UObject, public IRigVMClientHost, public IRigVMGraphFunctionHost
{
	GENERATED_BODY()

	UAnimNextParameterBlock_EditorData(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITORONLY_DATA
	ANIMNEXTUNCOOKEDONLY_API static const FName ExportsAssetRegistryTag; 

	friend class UAnimNextParameterBlockLibrary;
	friend class UAnimNextParameterBlockFactory;
	friend class UAnimNextParameterBlockEntry;
	friend class UAnimNextParameterBlock_EdGraph;
	friend class UAnimNextParameterBlockParameter;
	friend class UAnimNextParameterBlockBindingReference;
	friend struct UE::AnimNext::Editor::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;
	friend class UE::AnimNext::Editor::FParameterBlockEditor;
	friend struct FAnimNextParameterSchemaAction_RigUnit;
	friend struct FAnimNextParameterSchemaAction_DispatchFactory;
	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend class UE::AnimNext::Editor::SParameterPicker;
	friend class UE::AnimNext::Editor::FParameterBlockTabSummoner;
	friend class FAnimationAnimNextParametersEditorTest_Block;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend class UE::AnimNext::Editor::FParameterBlockParameterCustomization;

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockParameter* AddParameter(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockGraph* AddGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
	
	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockBinding* AddBinding(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockBindingReference* AddBindingReference(FName InName, UAnimNextParameterBlock* InBlock, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API bool RemoveAllBindings(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API bool RemoveEntry(UAnimNextParameterBlockEntry* InEntry, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API bool RemoveEntries(const TArray<UAnimNextParameterBlockEntry*>& InEntries, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockEntry* FindBinding(FName InName) const;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual bool IsEditorOnly() const override { return true; }
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

#if WITH_EDITOR
	void HandlePackageDone(const FEndLoadPackageContext& Context);
	void HandlePackageDone();
#endif

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
	virtual void RecompileVM() override;
	virtual void RecompileVMIfRequired() override;
	virtual void RequestAutoVMRecompilation() override;
	virtual void SetAutoVMRecompile(bool bAutoRecompile) override;
	virtual bool GetAutoVMRecompile() const override;
	virtual void IncrementVMRecompileBracket() override;
	virtual void DecrementVMRecompileBracket() override;
	virtual void RefreshAllModels(ERigVMLoadType InLoadType) override;
	virtual void OnRigVMRegistryChanged() override;
	virtual void RequestRigVMInit() override;
	virtual URigVMGraph* GetModel(const UEdGraph* InEdGraph = nullptr) const override;
	virtual URigVMGraph* GetModel(const FString& InNodePath) const override;
	virtual URigVMGraph* GetDefaultModel() const override;
	virtual TArray<URigVMGraph*> GetAllModels() const override;
	virtual URigVMFunctionLibrary* GetLocalFunctionLibrary() const override;
	virtual URigVMGraph* AddModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;
	virtual bool RemoveModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;
	virtual FRigVMGetFocusedGraph& OnGetFocusedGraph() override;
	virtual const FRigVMGetFocusedGraph& OnGetFocusedGraph() const override;
	virtual URigVMGraph* GetFocusedModel() const override;
	virtual URigVMController* GetController(const URigVMGraph* InGraph = nullptr) const override;
	virtual URigVMController* GetControllerByName(const FString InGraphName = TEXT("")) const override;
	virtual URigVMController* GetOrCreateController(URigVMGraph* InGraph = nullptr) override;
	virtual URigVMController* GetController(const UEdGraph* InEdGraph) const override;
	virtual URigVMController* GetOrCreateController(const UEdGraph* InGraph) override;
	virtual TArray<FString> GeneratePythonCommands(const FString InNewBlueprintName) override;


	// IRigVMGraphFunctionHost interface
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override;
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override;

	UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce);
	bool RemoveEdGraph(URigVMGraph* InModel);
	
	ANIMNEXTUNCOOKEDONLY_API void Initialize(bool bRecompileVM);

	void Recompile();

	void RecompileIfRequired();

	void RequestAutoRecompilation();
	
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode);

	ANIMNEXTUNCOOKEDONLY_API void BroadcastModified();

	void ReportError(const TCHAR* InMessage) const;

	void ReconstructAllNodes();
	
	UPROPERTY()
	TArray<TObjectPtr<UAnimNextParameterBlock_EdGraph>> Graphs;

	UPROPERTY()
	FRigVMClient RigVMClient;

	UPROPERTY()
	FRigVMGraphFunctionStore GraphFunctionStore;

	UPROPERTY(EditAnywhere, Category = "User Interface")
	FRigVMEdGraphDisplaySettings RigGraphDisplaySettings;

	UPROPERTY(EditAnywhere, Category = "VM")
	FRigVMRuntimeSettings VMRuntimeSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VM", meta = (AllowPrivateAccess = "true"))
	FRigVMCompileSettings VMCompileSettings;

	UPROPERTY(transient, DuplicateTransient)
	TMap<FString, FRigVMOperand> PinToOperandMap;

	/** All entries in this parameter block - not saved, either serialized or discovered at load time */
	UPROPERTY(transient)
	TArray<TObjectPtr<UAnimNextParameterBlockEntry>> Entries;

	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;

	UPROPERTY(transient, DuplicateTransient)
	int32 VMRecompilationBracket = 0;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bStructRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling = false;
	
	FCompilerResultsLog CompileLog;

	FOnRigVMCompiledEvent RigVMCompiledEvent;
	FRigVMGraphModifiedEvent RigVMGraphModifiedEvent;

	// Delegate to subscribe to modifications to this block
	UE::AnimNext::UncookedOnly::FOnParameterBlockModified ModifiedDelegate;

	bool bAutoRecompileVM = true;
	bool bErrorsDuringCompilation = false;
	bool bSuspendModelNotificationsForSelf = false;
	bool bSuspendModelNotificationsForOthers = false;
	bool bSuspendAllNotifications = false;
	bool bCompileInDebugMode = false;
	bool bSuspendPythonMessagesForRigVMClient = true;
	bool bSuspendBlockNotifications = false;
#endif // #if WITH_EDITORONLY_DATA
};
