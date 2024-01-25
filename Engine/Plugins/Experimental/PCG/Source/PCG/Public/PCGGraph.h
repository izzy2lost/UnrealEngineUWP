// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Graph/PCGStackContext.h"
#include "Helpers/PCGGraphParameterExtension.h"

#include "PropertyBag.h"
#include "UObject/ObjectPtr.h"

#include "PCGGraph.generated.h"

class UPCGGraphInterface;
#if WITH_EDITOR
class UPCGEditorGraph;
struct FEdGraphPinType;
#endif // WITH_EDITOR

enum class EPCGGraphParameterEvent
{
	GraphChanged,
	GraphPostLoad,
	Added,
	Removed,
	PropertyModified,
	ValueModifiedLocally,
	ValueModifiedByParent,
	MultiplePropertiesAdded
};

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPCGGraphChanged, UPCGGraphInterface* /*Graph*/, EPCGChangeType /*ChangeType*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphStructureChanged, UPCGGraphInterface* /*Graph*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPCGGraphParametersChanged, UPCGGraphInterface* /*Graph*/, EPCGGraphParameterEvent /*ChangeType*/, FName /*ChangedPropertyName*/);
#endif // WITH_EDITOR

/**
* Extended version of FInstancedPropertyBag, to support overrides and have a custom UI for it
* Must only be used with PCGGraphInstances.
* TODO: Should be made generic and moved to ScriptUtils.
*/
USTRUCT()
struct PCG_API FPCGOverrideInstancedPropertyBag
{
	GENERATED_BODY()

public:
	/** Add/Remove given property from overrides, and reset its value if it is removed. Returns true if the value was changed. */
	bool UpdatePropertyOverride(const FProperty* InProperty, bool bMarkAsOverridden, const FInstancedPropertyBag* ParentUserParameters);

	/** Reset overridden property to its parent value. */
	void ResetPropertyToDefault(const FProperty* InProperty, const FInstancedPropertyBag* ParentUserParameters);

	/** Return if the property is currently marked overridden. */
	bool IsPropertyOverridden(const FProperty* InProperty) const;

	/** Return if the property is currently marked overridden and has a different value than its default value. */
	bool IsPropertyOverriddenAndNotDefault(const FProperty* InProperty, const FInstancedPropertyBag* ParentUserParameters) const;

	/** Reset the struct. */
	void Reset();

	/** Return if the parameters are valid */
	bool IsValid() { return Parameters.IsValid(); }

	/** Will migrate to a new property bag instanced, and will remove porperties that doesn't exists anymore or have changed types. */
	void MigrateToNewBagInstance(const FInstancedPropertyBag& NewBagInstance);

	/** Handle the sync between the parent parameters and the overrides. Will return true if something changed. */
	bool RefreshParameters(const FInstancedPropertyBag* ParentUserParameters, EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);

	UPROPERTY(EditAnywhere, Category = "")
	FInstancedPropertyBag Parameters;

	UPROPERTY(VisibleAnywhere, Category = "", meta = (Hidden))
	TSet<FGuid> PropertiesIDsOverridden;
};

UCLASS(BlueprintType, Abstract, ClassGroup = (Procedural))
class PCG_API UPCGGraphInterface : public UObject
{
	GENERATED_BODY()

public:
	/** Return the underlying PCG Graph for this interface. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGGraph* GetMutablePCGGraph() { return GetGraph(); }

	/** Return the underlying PCG Graph for this interface. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	const UPCGGraph* GetConstPCGGraph() const { return GetGraph(); }

	virtual UPCGGraph* GetGraph() PURE_VIRTUAL(UPCGGraphInterface::GetGraph, return nullptr;)
	virtual const UPCGGraph* GetGraph() const PURE_VIRTUAL(UPCGGraphInterface::GetGraph, return nullptr;)

	virtual const FInstancedPropertyBag* GetUserParametersStruct() const PURE_VIRTUAL(UPCGGraphInterface::GetUserParametersStruct, return nullptr;)

	// Mutable version should not be used outside of testing, since there are callbacks fired when parameters changes.
	// TODO: Make it safe to change parameters from the outside.
	FInstancedPropertyBag* GetMutableUserParametersStruct_Unsafe() const { return const_cast<FInstancedPropertyBag*>(GetUserParametersStruct()); }

	bool IsInstance() const;

	/** A graph interface is equivalent to another graph interface if they are the same (same ptr), or if they have the same graph. Will be overridden when graph instance supports overrides. */
	virtual bool IsEquivalent(const UPCGGraphInterface* Other) const;

#if WITH_EDITOR
	FOnPCGGraphChanged OnGraphChangedDelegate;
	FOnPCGGraphParametersChanged OnGraphParametersChangedDelegate;

	/** A structural change has been made such as changing a static branch or a higen grid size node. */
	FOnPCGGraphStructureChanged OnGraphStructureChangedDelegate;
#endif // WITH_EDITOR

	template <typename T>
	TValueOrError<T, EPropertyBagResult> GetGraphParameter(const FName PropertyName) const
	{
		const FInstancedPropertyBag* UserParameters = GetUserParametersStruct();
		check(UserParameters);

		if constexpr (std::is_enum_v<T> && StaticEnum<T>())
		{
			return FPCGGraphParameterExtension::GetGraphParameter<T>(*UserParameters, PropertyName, StaticEnum<T>());
		}
		else
		{
			return FPCGGraphParameterExtension::GetGraphParameter<T>(*UserParameters, PropertyName);
		}
	}

	virtual bool IsGraphParameterOverridden(const FName PropertyName) const { return false; }

	template <typename T>
	EPropertyBagResult SetGraphParameter(const FName PropertyName, const T& Value)
	{
		FInstancedPropertyBag* UserParameters = GetMutableUserParametersStruct();
		check(UserParameters);

		EPropertyBagResult Result;
		if constexpr (std::is_enum_v<T> && StaticEnum<T>())
		{
			Result = FPCGGraphParameterExtension::SetGraphParameter(*UserParameters, PropertyName, Value, StaticEnum<T>());
		}
		else
		{
			Result = FPCGGraphParameterExtension::SetGraphParameter<T>(*UserParameters, PropertyName, Value);
		}

		if (Result == EPropertyBagResult::Success)
		{
			OnGraphParametersChanged(EPCGGraphParameterEvent::ValueModifiedLocally, PropertyName);
		}

		return Result;
	}

	EPropertyBagResult SetGraphParameter(const FName PropertyName, const uint64 Value, const UEnum* Enum);

protected:
	virtual void OnGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName) PURE_VIRTUAL(UPCGGraphInterface::OnGraphParametersChanged, )
	virtual FInstancedPropertyBag* GetMutableUserParametersStruct() PURE_VIRTUAL(UPCGGraphInterface::GetMutableUserParametersStruct, return nullptr;)
};

UCLASS(BlueprintType, ClassGroup = (Procedural), hidecategories=(Object))
class PCG_API UPCGGraph : public UPCGGraphInterface
{
#if WITH_EDITOR
	friend class FPCGEditor;
	friend class FPCGSubgraphHelpers;
#endif // WITH_EDITOR

	GENERATED_BODY()

public:
	UPCGGraph(const FObjectInitializer& ObjectInitializer);
	/** ~Begin UObject interface */
	virtual void PostLoad() override;
	virtual bool IsEditorOnly() const override;
#if WITH_EDITOR
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	virtual void BeginDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* InProperty) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	/** ~End UObject interface */

	/** ~Begin UPCGGraphInterface interface */
	virtual UPCGGraph* GetGraph() override { return this; }
	virtual const UPCGGraph* GetGraph() const override { return this; }
	/** ~End UPCGGraphInterface interface */

	// Default grid size for generation. For hierarchical generation, nodes outside of grid size graph ranges will generate on this grid.
	EPCGHiGenGrid GetDefaultGrid() const { ensure(IsHierarchicalGenerationEnabled()); return HiGenGridSize; }
	uint32 GetDefaultGridSize() const;
	bool IsHierarchicalGenerationEnabled() const { return bUseHierarchicalGeneration; }

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Category;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Description;
#endif

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bLandscapeUsesMetadata = true;

	/** Creates a node using the given settings interface. Does not manage ownership - done outside of this method. */
	UPCGNode* AddNode(UPCGSettingsInterface* InSettings);

	/** Creates a default node based on the settings class wanted. Returns the newly created node. */
	UFUNCTION(BlueprintCallable, Category = Graph, meta=(DeterminesOutputType = "InSettingsClass", DynamicOutputParam = "DefaultNodeSettings"))
	UPCGNode* AddNodeOfType(TSubclassOf<class UPCGSettings> InSettingsClass, UPCGSettings*& DefaultNodeSettings);

	template <typename T, typename = typename std::enable_if_t<std::is_base_of_v<UPCGSettings, T>>>
	UPCGNode* AddNodeOfType(T*& DefaultNodeSettings);

	/** Creates a node containing an instance to the given settings. Returns the created node. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddNodeInstance(UPCGSettings* InSettings);

	/** Creates a node and copies the input settings. Returns the created node. */
	UFUNCTION(BlueprintCallable, Category = Graph, meta = (DeterminesOutputType = "InSettings", DynamicOutputParam = "OutCopiedSettings"))
	UPCGNode* AddNodeCopy(UPCGSettings* InSettings, UPCGSettings*& DefaultNodeSettings);

	/** Removes a node from the graph. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	void RemoveNode(UPCGNode* InNode);

	/** Bulk removal of nodes, to avoid notifying the world everytime. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	void RemoveNodes(TArray<UPCGNode*>& InNodes);

	/** Adds a directed edge in the graph. Returns the "To" node for easy chaining */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddEdge(UPCGNode* From, const FName& FromPinLabel, UPCGNode* To, const FName& ToPinLabel);

	/** Removes an edge in the graph. Returns true if an edge was removed. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	bool RemoveEdge(UPCGNode* From, const FName& FromLabel, UPCGNode* To, const FName& ToLabel);

	/** Returns the graph input node */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* GetInputNode() const { return InputNode; }

	/** Returns the graph output node */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* GetOutputNode() const { return OutputNode; }

	/** Duplicate a given node by creating a new node with the same settings and properties, but without any edges and add it to the graph */
	TObjectPtr<UPCGNode> ReconstructNewNode(const UPCGNode* InNode);

	/** Creates an edge between two nodes/pins based on the labels. Returns true if the To node has removed other edges (happens with single pins) */
	bool AddLabeledEdge(UPCGNode* From, const FName& InboundLabel, UPCGNode* To, const FName& OutboundLabel);

	/** Returns true if the current graph contains directly the specified node. This does not query recursively (through subgraphs). */
	bool Contains(UPCGNode* Node) const;

	/** Returns true if the current graph contains a subgraph node using statically the specified graph, recursively. */
	bool Contains(const UPCGGraph* InGraph) const;

	/** Returns the node with the given settings in the graph, if any */
	UPCGNode* FindNodeWithSettings(const UPCGSettingsInterface* InSettings, bool bRecursive = false) const;

	const TArray<UPCGNode*>& GetNodes() const { return Nodes; }
	void AddNode(UPCGNode* InNode);
	void AddNodes(TArray<UPCGNode*>& InNodes);

	/** Calls the lambda on every node in the graph or until the Action call returns false */
	bool ForEachNode(TFunctionRef<bool(UPCGNode*)> Action) const;

	/** Calls the lambda on every node (going through subgraphs too) or until the Action call returns false */
	bool ForEachNodeRecursively(TFunctionRef<bool(UPCGNode*)> Action) const;

	bool RemoveInboundEdges(UPCGNode* InNode, const FName& InboundLabel);
	bool RemoveOutboundEdges(UPCGNode* InNode, const FName& OutboundLabel);

	/** Determine the relevant grid sizes by inspecting all HiGenGridSize nodes. */
	void GetGridSizes(PCGHiGenGrid::FSizeArray& OutGridSizes, bool& bOutHasUnbounded) const;

#if WITH_EDITOR
	void DisableNotificationsForEditor();
	void EnableNotificationsForEditor();
	void ToggleUserPausedNotificationsForEditor();
	bool NotificationsForEditorArePausedByUser() const { return bUserPausedNotificationsInGraphEditor; }

	UFUNCTION(BlueprintCallable, Category = "Graph|Advanded")
	void ForceNotificationForEditor();

	void PreNodeUndo(UPCGNode* InPCGNode);
	void PostNodeUndo(UPCGNode* InPCGNode);

	const TArray<TObjectPtr<UObject>>& GetExtraEditorNodes() const { return ExtraEditorNodes; }
	void SetExtraEditorNodes(const TArray<TObjectPtr<const UObject>>& InNodes);

	bool IsInspecting() const { return bIsInspecting; }
	void EnableInspection() { bIsInspecting = true; }
	void DisableInspection() { bIsInspecting = false; }
	bool DebugFlagAppliesToIndividualComponents() const { return bDebugFlagAppliesToIndividualComponents; }
	void RemoveExtraEditorNode(const UObject* InNode);
#endif

#if WITH_EDITOR
	FPCGSelectionKeyToSettingsMap GetTrackedActorKeysToSettings() const;
	void GetTrackedActorKeysToSettings(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const;
#endif

	/** Size of grid on which this node should be executed. Nodes execute at the minimum of all input grid sizes. */
	uint32 GetNodeGenerationGridSize(const UPCGNode* InNode, uint32 InDefaultGridSize) const;

protected:
	void OnNodeAdded(UPCGNode* InNode);
	void OnNodesAdded(TArrayView<UPCGNode*> InNodes);
	void OnNodeRemoved(UPCGNode* InNode);
	void OnNodesRemoved(TArrayView<UPCGNode*> InNodes);

	void RemoveNodes_Internal(TArrayView<UPCGNode*> InNodes);
	void AddNodes_Internal(TArrayView<UPCGNode*> InNodes);

	bool IsEditorOnly_Internal() const;

	bool ForEachNodeRecursively_Internal(TFunctionRef<bool(UPCGNode*)> Action, TSet<const UPCGGraph*>& VisitedGraphs) const;

	/** Calculates node grid size. Not thread safe, must be called within write lock. */
	uint32 CalculateNodeGridSizeRecursive_Unsafe(const UPCGNode* InNode, uint32 InDefaultGridSize) const;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Graph, meta = (NoResetToDefault))
	TArray<TObjectPtr<UPCGNode>> Nodes;

	// Add input/output nodes
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Graph, meta = (NoResetToDefault))
	TObjectPtr<UPCGNode> InputNode;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Graph, meta = (NoResetToDefault))
	TObjectPtr<UPCGNode> OutputNode;

#if WITH_EDITORONLY_DATA
	// Extra data to hold information that is useful only in editor (like comments)
	UPROPERTY()
	TArray<TObjectPtr<UObject>> ExtraEditorNodes;

	// Editor graph created from PCG Editor but owned by this, reference is collected using AddReferencedObjects
	TObjectPtr<UPCGEditorGraph> PCGEditorGraph = nullptr;
#endif // WITH_EDITORONLY_DATA

	// Parameters
	UPROPERTY(EditAnywhere, Category = Instance, meta = (DisplayName = "Parameters", NoResetToDefault, DefaultType = "EPropertyBagPropertyType::Double", IsPinTypeAccepted = "UserParametersIsPinTypeAccepted", CanRemoveProperty = "UserParametersCanRemoveProperty"))
	FInstancedPropertyBag UserParameters;

#if WITH_EDITOR
	UFUNCTION(BlueprintInternalUseOnly)
	bool UserParametersIsPinTypeAccepted(FEdGraphPinType InPinType, bool bIsChild);

	UFUNCTION(BlueprintInternalUseOnly)
	bool UserParametersCanRemoveProperty(FGuid InPropertyID, FName InPropertyName);
#endif // WITH_EDITOR

	virtual FInstancedPropertyBag* GetMutableUserParametersStruct() override;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bUseHierarchicalGeneration = false;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (DisplayName = "HiGen Default Grid Size", EditCondition = "bUseHierarchicalGeneration"))
	EPCGHiGenGrid HiGenGridSize = EPCGHiGenGrid::Grid256;

	/** Execution grid size for nodes. */
	mutable TMap<const UPCGNode*, uint32> NodeToGridSize;
	mutable FRWLock NodeToGridSizeLock;

	/** Sets whether this graph is marked as editor-only; note that the IsEditorOnly call depends on the local graph value and the value in all subgraphs, recursively. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Cooking)
	bool bIsEditorOnly = false;

#if WITH_EDITORONLY_DATA
	/** When true the Debug flag in the graph editor will display debug information contextually for the selected debug object. Otherwise
	* debug information is displayed for all components using a graph (requires regenerate).
	*/
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebugFlagAppliesToIndividualComponents = true;
#endif // WITH_EDITORONLY_DATA

public:
	virtual const FInstancedPropertyBag* GetUserParametersStruct() const override { return &UserParameters; }

	// Add new user parameters using an array of descriptors. Can also provide an original graph to copy the values.
	// Original Graph needs to have the properties.
	// Be careful if there is any overlap between existing parameters, that also exists in the original graph, they will be overridden by the original.
	// Best used on a brand new PCG Graph.
	void AddUserParameters(const TArray<FPropertyBagPropertyDesc>& InDescs, const UPCGGraph* InOptionalOriginalGraph = nullptr);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	FPCGRuntimeGenerationRadii GenerationRadii;

protected:
	virtual void OnGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName) override;

#if WITH_EDITOR
private:
	void NotifyGraphChanged(EPCGChangeType ChangeType);
	void OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType);

	void NotifyGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);

	/** Remove invalid edges and edges to nodes that are not present in the node array. */
	void FixInvalidEdges();

	int32 GraphChangeNotificationsDisableCounter = 0;
	bool bDelayedChangeNotification = false;
	EPCGChangeType DelayedChangeType = EPCGChangeType::None;
	bool bIsNotifying = false;
	bool bUserPausedNotificationsInGraphEditor = false;
	int32 NumberOfUserParametersPreEdit = 0;
	bool bIsInspecting = false;
#endif // WITH_EDITOR
};

UCLASS(BlueprintType, ClassGroup = (Procedural), hidecategories = (Object))
class PCG_API UPCGGraphInstance : public UPCGGraphInterface
{
	GENERATED_BODY()
public:
	/** ~Begin UPCGGraphInterface interface */
	virtual UPCGGraph* GetGraph() override { return Graph ? Graph->GetGraph() : nullptr; }
	virtual const UPCGGraph* GetGraph() const override { return Graph ? Graph->GetGraph() : nullptr; }
	/** ~End UPCGGraphInterface interface */

	// ~Begin UObject interface
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void BeginDestroy() override;
	virtual bool IsEditorOnly() const override { return GetGraph() && GetGraph()->IsEditorOnly(); }

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* InProperty) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	// ~End UObject interface

	void SetupCallbacks();
	void TeardownCallbacks();
#endif

protected:
#if WITH_EDITOR
	void OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);
	void OnGraphStructureChanged(UPCGGraphInterface* InGraph);
	void NotifyGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);
#endif
	void OnGraphParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);
	virtual void OnGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName) override;
	void RefreshParameters(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName = NAME_None);
	virtual FInstancedPropertyBag* GetMutableUserParametersStruct() override;

public:
	static TObjectPtr<UPCGGraphInterface> CreateInstance(UObject* InOwner, UPCGGraphInterface* InGraph);

	void SetGraph(UPCGGraphInterface* InGraph);
	void CopyParameterOverrides(UPCGGraphInterface* InGraph);
	void UpdatePropertyOverride(const FProperty* InProperty, bool bMarkAsOverridden);
	void ResetPropertyToDefault(const FProperty* InProperty);
	bool IsPropertyOverridden(const FProperty* InProperty) const { return ParametersOverrides.IsPropertyOverridden(InProperty); }
	bool IsPropertyOverriddenAndNotDefault(const FProperty* InProperty) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Instance)
	TObjectPtr<UPCGGraphInterface> Graph;

	UPROPERTY(EditAnywhere, Category = Instance, meta = (NoResetToDefault))
	FPCGOverrideInstancedPropertyBag ParametersOverrides;

	virtual const FInstancedPropertyBag* GetUserParametersStruct() const override { return &ParametersOverrides.Parameters; }
	virtual bool IsGraphParameterOverridden(const FName PropertyName) const override;

private:
#if WITH_EDITORONLY_DATA
	// Transient, to keep track the undo/redo changed the graph.
	UPCGGraphInterface* UndoRedoGraphCache = nullptr;
#endif // WITH_EDITORONLY_DATA
};

template <typename T, typename>
UPCGNode* UPCGGraph::AddNodeOfType(T*& DefaultNodeSettings)
{
	UPCGSettings* TempSettings = DefaultNodeSettings;
	UPCGNode* Node = AddNodeOfType(T::StaticClass(), TempSettings);
	DefaultNodeSettings = Cast<T>(TempSettings);
	return Node;
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
