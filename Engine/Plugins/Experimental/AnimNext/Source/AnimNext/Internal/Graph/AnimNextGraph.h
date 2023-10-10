// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVM.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/EntryPointHandle.h"
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

	// If the graph instance is allocated, we release it during destruction
	~FAnimNextGraphInstance();

	// Releases the graph instance and frees all corresponding memory
	void Release();

	// Returns true if we have a live graph instance, false otherwise
	bool IsValid() const;

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
class ANIMNEXT_API UAnimNextGraph : public UObject
{
	GENERATED_BODY()

public:
	// UObject interface
	virtual void PostLoad() override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// Allocates an instance of the graph, retain the handle and use it with the Run() function to evaluate it
	void AllocateInstance(FAnimNextGraphInstance& Instance) const;

	// Run the specified simulation steps on the provided graph with the given context
	void Run(const UE::AnimNext::FContext& Context, FAnimNextGraphInstance& GraphInstance, EAnimNextGraphSimulationSteps SimulationSteps) const;

protected:
	// Support rig VM execution
	TArray<FRigVMExternalVariable> GetRigVMExternalVariables();

	// Loads the graph data from the provided archive buffer and returns true on success, false otherwise
	bool LoadFromArchiveBuffer(const TArray<uint8>& SharedDataArchiveBuffer);
	
	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend struct FAnimNextGraphInstance;
	friend class UAnimGraphNode_AnimNextGraph;

	// This is a handle to the root decorator in our graph
	UPROPERTY()
	FAnimNextEntryPointHandle RootDecoratorHandle;

	// This is a resolved handle to the root decorator in our graph
	FAnimNextDecoratorHandle ResolvedRootDecoratorHandle;

	// This is the graph shared data used by the decorator system, the output of FDecoratorReader
	// We de-serialize manually into this buffer from the archive buffer, this is never saved on disk
	TArray<uint8> SharedDataBuffer;

	// This is the list of all referenced UObjects in the graph shared data
	// We collect all the references here to make it quick and easy for the GC to query them
	// It means that object references in the graph shared data are not visited at runtime by the GC (they are immutable)
	TArray<TObjectPtr<UObject>> TrackedObjectsForGC;

	// The RigVM object holds the bytecode, literals, etc used by the RigVM internals, there is a single instance along with the UAnimNextGraph (1:1 mapping)
	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	// The ExtendedExecuteContext object holds instance/work data used by the RigVM internals. It is populated during compilation same as the URigVM object above.
	// We also have a 1:1 mapping with the UAnimNextGraph but each instance of the anim graph also needs its own exetended execute context object. This one is used
	// as a reference we copy from.
	UPROPERTY()
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	UPROPERTY()
	FRigVMRuntimeSettings VMRuntimeSettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Graph", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;

	// This buffer holds the output of the FDecoratorWriter post compilation
	// We serialize it manually and it is discarded at runtime
	TArray<uint8> SharedDataArchiveBuffer;
#endif
};
