// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/AnimNextGraphEntryPoint.h"
#include "AnimNextRigVMAsset.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "RigVMCore/RigVM.h"
#include "TraitCore/TraitPtr.h"
#include "TraitCore/TraitHandle.h"
#include "TraitCore/EntryPointHandle.h"
#include "Graph/GraphInstanceComponent.h"
#include "Graph/RigUnit_AnimNextGraphEvaluator.h"
#include "Param/ParamId.h"
#include "Scheduler/IAnimNextScheduleTermInterface.h"
#include "RigVMHost.h"

#include "AnimNextModule.generated.h"

class UEdGraph;
class UAnimNextModule;
class UAnimGraphNode_AnimNextGraph;
struct FAnimNode_AnimNextGraph;
struct FRigUnit_AnimNextGraphEvaluator;
struct FAnimNextGraphInstancePtr;
struct FAnimNextGraphInstance;
class UAnimNextSchedule;
struct FAnimNextScheduleGraphTask;
struct FAnimNextEditorParam;
struct FAnimNextParam;

namespace UE::AnimNext
{
	struct FContext;
	struct FExecutionContext;
	class FAnimNextModuleImpl;
	struct FTestUtils;
	struct FParametersProxy;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FModuleEditor;
	class FParameterCustomization;
}

namespace UE::AnimNext::Graph
{
	extern ANIMNEXT_API const FName EntryPointName;
	extern ANIMNEXT_API const FName ResultName;
}

// A user-created collection of animation logic & data
UCLASS(BlueprintType)
class ANIMNEXT_API UAnimNextModule : public UAnimNextRigVMAsset, public IAnimNextScheduleTermInterface
{
	GENERATED_BODY()

public:
	UAnimNextModule(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// IAnimNextScheduleTermInterface interface
	virtual TConstArrayView<UE::AnimNext::FScheduleTerm> GetTerms() const override;

	// Allocates an instance of the graph
	// @param	OutInstance		The instance to allocate data for
	// @param	InEntryPoint	The entry point to use. If this is NAME_None then the default entry point for this graph is used
	void AllocateInstance(FAnimNextGraphInstancePtr& OutInstance, FName InEntryPoint = NAME_None) const;

	// Allocates an instance of the graph with the specified parent graph instance
	// @param	InOutParentGraphInstance	The parent graph instance to use
	// @param	OutInstance					The instance to allocate data for
	// @param	InEntryPoint				The entry point to use. If this is NAME_None then the default entry point for this graph is used
	void AllocateInstance(FAnimNextGraphInstance& InOutParentGraphInstance, FAnimNextGraphInstancePtr& OutInstance, FName InEntryPoint = NAME_None) const;

	// Update the parameter layer, if any
	void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle, float InDeltaTime) const;

	// Get the fully-qualified default entry point name (/Path/To/Asset.Asset:EntryPoint)
	FName GetDefaultEntryPoint() const;

protected:

	// Loads the graph data from the provided archive buffer and returns true on success, false otherwise
	bool LoadFromArchiveBuffer(const TArray<uint8>& SharedDataArchiveBuffer);

	// Allocates an instance of the graph with an optional parent graph instance
	void AllocateInstanceImpl(FAnimNextGraphInstance* InOutParentGraphInstance, FAnimNextGraphInstancePtr& OutInstance, FName InEntryPoint) const;

#if WITH_EDITORONLY_DATA
	// During graph compilation, if we have existing graph instances, we freeze them by releasing their memory before thawing them
	// Freezing is a partial release of resources that retains the necessary information to re-create things safely
	void FreezeGraphInstances();

	// During graph compilation, once compilation is done we thaw existing graph instances to reallocate their memory
	void ThawGraphInstances();
#endif

	// Set the default entry point name (unqualified)
	void SetDefaultEntryPoint(FName InEntryPoint);

	// Cache the default entry point
	void CacheDefaultEntryPoint() const;

	friend class UAnimNextModuleFactory;
	friend class UAnimNextModule_EditorData;
	friend class UAnimNextModule_Parameter;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FModuleEditor;
	friend struct UE::AnimNext::FTestUtils;
	friend FAnimNextGraphInstancePtr;
	friend FAnimNextGraphInstance;
	friend class UAnimGraphNode_AnimNextGraph;
	friend UE::AnimNext::FExecutionContext;
	friend class UAnimNextSchedule;
	friend struct FAnimNextScheduleGraphTask;
	friend UE::AnimNext::FAnimNextModuleImpl;
	friend class UE::AnimNext::Editor::FParameterCustomization;
	friend struct UE::AnimNext::FParametersProxy;

#if WITH_EDITORONLY_DATA
	mutable FCriticalSection GraphInstancesLock;

	// This is a list of live graph instances that have been allocated, used in the editor to reset instances when we re-compile/live edit
	mutable TSet<FAnimNextGraphInstance*> GraphInstances;
#endif

	// This is the execute method definition used by a graph to evaluate latent pins
	UPROPERTY()
	FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;

	// Data for each entry point in this graph
	UPROPERTY()
	TArray<FAnimNextGraphEntryPoint> EntryPoints;

	// This is a resolved handle to the root trait in our graph, for each entry point 
	TMap<FName, FAnimNextTraitHandle> ResolvedRootTraitHandles;

	// This is an index into EntryPoints, for each entry point
	TMap<FName, int32> ResolvedEntryPoints;

	// This is the graph shared data used by the trait system, the output of FTraitReader
	// We de-serialize manually into this buffer from the archive buffer, this is never saved on disk
	TArray<uint8> SharedDataBuffer;

	// This is a list of all referenced UObjects in the graph shared data
	// We collect all the references here to make it quick and easy for the GC to query them
	// It means that object references in the graph shared data are not visited at runtime by the GC (they are immutable)
	// The shared data serialization archive stores indices to these to perform UObject serialization
	UPROPERTY()
	TArray<TObjectPtr<UObject>> GraphReferencedObjects;

	// The entry point that this graph defaults to using (unqualified by asset). Use GetDefaultEntryPoint to get the fully-qualified name.
	UPROPERTY(EditAnywhere, Category = "Graph")
	FName DefaultEntryPoint = FRigUnit_AnimNextGraphRoot::DefaultEntryPoint;

	// Cached fully-qualified entry point, set via by SetDefaultEntryPoint/GetDefaultEntryPoint.
	UPROPERTY(Transient)
	mutable FName CachedDefaultEntryPoint = NAME_None;

	// Hash of required parameters
	UPROPERTY()
	uint64 RequiredParametersHash = 0;

	// All the parameters that are required for this graph to run
	UPROPERTY()
	TArray<FAnimNextParam> RequiredParameters;

	// Default state for this graph
	UPROPERTY()
	FAnimNextGraphState DefaultState;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FInstancedPropertyBag PropertyBag_DEPRECATED;

	// This buffer holds the output of the FTraitWriter post compilation
	// We serialize it manually and it is discarded at runtime
	TArray<uint8> SharedDataArchiveBuffer;
#endif
};
