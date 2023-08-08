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

// A user-created graph of logic used to supply data
UCLASS(BlueprintType)
class ANIMNEXT_API UAnimNextGraph : public UObject
{
	GENERATED_BODY()

public:
	// UObject interface
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// Allocates an instance of the graph, retain the handle and use it with the Run() function to evaluate it
	UE::AnimNext::FDecoratorPtr AllocateInstance() const;

	// Releases an instance of the graph and clears the provided handle
	void ReleaseInstance(UE::AnimNext::FDecoratorPtr& GraphInstancePtr) const;

	// Run the specified simulation steps on the provided graph with the given context
	void Run(const UE::AnimNext::FContext& Context, UE::AnimNext::FWeakDecoratorPtr GraphInstancePtr, EAnimNextGraphSimulationSteps SimulationSteps) const;

protected:
	// Support rig VM execution
	TArray<FRigVMExternalVariable> GetRigVMExternalVariables();

	// Loads the graph data from the provided archive buffer and returns true on success, false otherwise
	bool LoadFromArchiveBuffer(const TArray<uint8>& SharedDataArchiveBuffer);
	
	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend class UAnimNextGraph;
	friend class UAnimGraphNode_AnimNextGraph;

	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

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

	UPROPERTY(transient)
	mutable FRigVMExtendedExecuteContext ExtendedExecuteContext;

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
