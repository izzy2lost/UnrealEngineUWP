// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVM.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/EntryPointHandle.h"
#include "Graph/RigUnit_AnimNextGraphEvaluator.h"
#include "Param/ParamId.h"
#include "Scheduler/IAnimNextScheduleTermInterface.h"
#include "RigVMHost.h"

#include "AnimNextGraph.generated.h"

class UEdGraph;
class UAnimNextGraph;
class UAnimGraphNode_AnimNextGraph;
struct FAnimNode_AnimNextGraph;
enum class EAnimNextGraphSimulationSteps;

namespace UE::AnimNext
{
	struct FContext;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FGraphEditor;
}

namespace UE::AnimNext::Graph
{
	extern ANIMNEXT_API const FName EntryPointName;
	extern ANIMNEXT_API const FName ResultName;
}

// Represents an instance of an AnimNext graph
// This struct uses UE reflection because we wish for the GC to keep the graph
// alive while we own a reference to it. It is not intended to be serialized on disk with a live instance.
USTRUCT()
struct ANIMNEXT_API FAnimNextGraphInstance
{
	GENERATED_BODY()

	// Creates an empty graph instance that doesn't reference anything
	FAnimNextGraphInstance() = default;

#if WITH_EDITORONLY_DATA
	// In editor, we need custom copy/move semantics to update the live instance tracking
	FAnimNextGraphInstance(const FAnimNextGraphInstance& Other);
	FAnimNextGraphInstance(FAnimNextGraphInstance&& Other);
	FAnimNextGraphInstance& operator=(const FAnimNextGraphInstance& Other);
	FAnimNextGraphInstance& operator=(FAnimNextGraphInstance&& Other);
#else
	FAnimNextGraphInstance(const FAnimNextGraphInstance&) = default;
	FAnimNextGraphInstance& operator=(const FAnimNextGraphInstance&) = default;
	FAnimNextGraphInstance(FAnimNextGraphInstance&&) = default;
	FAnimNextGraphInstance& operator=(FAnimNextGraphInstance&&) = default;
#endif

	// If the graph instance is allocated, we release it during destruction
	~FAnimNextGraphInstance();

	// Releases the graph instance and frees all corresponding memory
	void Release();

	// Returns true if we have a live graph instance, false otherwise
	bool IsValid() const;

	// Check to see if this instance data matches the provided graph
	bool UsesGraph(const UAnimNextGraph* InGraph) const;

private:
	// Hard reference to the graph used to create this instance to ensure we can release it safely
	UPROPERTY()
	TObjectPtr<const UAnimNextGraph> Graph;

	// Hard reference to the graph instance data, we own it
	UE::AnimNext::FDecoratorPtr GraphInstancePtr;

	// Extended execute context instance for this graph instance, we own it
	UPROPERTY()
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	// The graph is the one that allocates instances
	friend UAnimNextGraph;
};

// A user-created graph of logic used to supply data
UCLASS(BlueprintType)
class ANIMNEXT_API UAnimNextGraph :  public URigVMHost, public IAnimNextScheduleTermInterface
{
	GENERATED_BODY()

public:
	UAnimNextGraph(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	// IAnimNextScheduleTermInterface interface
	virtual TConstArrayView<UE::AnimNext::FScheduleTerm> GetTerms() const override;

	// Allocates an instance of the graph, retain the handle and use it with the Run() function to evaluate it
	void AllocateInstance(FAnimNextGraphInstance& Instance) const;

	// Run the specified simulation steps on the provided graph with the given context
	void Run(const UE::AnimNext::FContext& Context, FAnimNextGraphInstance& GraphInstance, EAnimNextGraphSimulationSteps SimulationSteps) const;

	// Get the parameter to use to access the reference pose
	UE::AnimNext::FParamId GetReferencePoseParam() const { return ReferencePoseId; }

	// Get the parameter to use to access the current LOD
	UE::AnimNext::FParamId GetCurrentLODParam() const { return CurrentLODId; }

protected:
	// Support rig VM execution
	TArray<FRigVMExternalVariable> GetRigVMExternalVariables();

	// Loads the graph data from the provided archive buffer and returns true on success, false otherwise
	bool LoadFromArchiveBuffer(const TArray<uint8>& SharedDataArchiveBuffer);

#if WITH_EDITORONLY_DATA
	// Releases all live graph instances and returns a list of the instances that were released
	TArray<FAnimNextGraphInstance*> ReleaseAllInstances();
#endif

	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend struct FAnimNextGraphInstance;
	friend class UAnimGraphNode_AnimNextGraph;

#if WITH_EDITORONLY_DATA
	mutable FRWLock GraphInstancesLock;

	// This is a list of live graph instances that have been allocated, used in the editor to reset instances when we re-compile/live edit
	mutable TSet<FAnimNextGraphInstance*> GraphInstances;
#endif

	// This is the execute method definition used by this graph
	UPROPERTY()
	FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;

	// This is a handle to the root decorator in our graph
	UPROPERTY()
	FAnimNextEntryPointHandle RootDecoratorHandle;

	// This is a resolved handle to the root decorator in our graph
	FAnimNextDecoratorHandle ResolvedRootDecoratorHandle;

	// This is the graph shared data used by the decorator system, the output of FDecoratorReader
	// We de-serialize manually into this buffer from the archive buffer, this is never saved on disk
	TArray<uint8> SharedDataBuffer;

	// This is a list of all referenced UObjects in the graph shared data
	// We collect all the references here to make it quick and easy for the GC to query them
	// It means that object references in the graph shared data are not visited at runtime by the GC (they are immutable)
	// The shared data serialization archive stores indices to these to perform UObject serialization
	UPROPERTY()
	TArray<TObjectPtr<UObject>> GraphReferencedObjects;

	// The RigVM object holds the bytecode, literals, etc used by the RigVM internals, there is a single instance along with the UAnimNextGraph (1:1 mapping)
	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	// The ExtendedExecuteContext object holds the common work data used by the RigVM internals. It is populated during the initial VM initialization.
	// Each instance of an AnimGraph requires a copy of this context and a call to initialize the VM instance with the context copy, 
	// so the cached memory handles are updated to the correct memory addresses.
	// This context is used as a reference to copy the common data for all instances created.
	UPROPERTY(Transient)
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	// The parameter to use to access the reference pose
	UPROPERTY(EditAnywhere, Category = "Graph", meta=(CustomWidget = "ParamName", AllowedParamType = "FAnimNextGraphReferencePose"))
	FName ReferencePose = TEXT("UE_AnimNextMeshComponent_ReferencePose");

	// The parameter to use to access the current LOD
	UPROPERTY(EditAnywhere, Category = "Graph", meta=(CustomWidget = "ParamName", AllowedParamType = "int32"))
	FName CurrentLOD = TEXT("UE_AnimNextMeshComponent_PredictedLODLevel");

	UE::AnimNext::FParamId ReferencePoseId = UE::AnimNext::FParamId(ReferencePose);
	UE::AnimNext::FParamId CurrentLODId = UE::AnimNext::FParamId(CurrentLOD);

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Graph", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;

	// This buffer holds the output of the FDecoratorWriter post compilation
	// We serialize it manually and it is discarded at runtime
	TArray<uint8> SharedDataArchiveBuffer;
#endif
};
