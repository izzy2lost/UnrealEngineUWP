// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Param/ParamTypeHandle.h"
#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/ExecutionContext.h"
#include "Serialization/MemoryReader.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextGraph)

namespace UE::AnimNext::Graph
{
const FName EntryPointName("GetData");
const FName ResultName("Result");
}

UE::AnimNext::FDecoratorPtr UAnimNextGraph::AllocateInstance() const
{
	if (!ResolvedRootDecoratorHandle.IsValid())
	{
		return UE::AnimNext::FDecoratorPtr();
	}

	UE::AnimNext::FExecutionContext Context(SharedDataBuffer);

	return Context.AllocateNodeInstance(UE::AnimNext::FWeakDecoratorPtr(), ResolvedRootDecoratorHandle);
}

void UAnimNextGraph::ReleaseInstance(UE::AnimNext::FDecoratorPtr& GraphInstancePtr) const
{
	if (!GraphInstancePtr.IsValid())
	{
		return;
	}

	// We need an execution context for this graph to be active when we delete the graph instance
	UE::AnimNext::FExecutionContext Context(SharedDataBuffer);

	GraphInstancePtr.Reset();
}

void UAnimNextGraph::Run(const UE::AnimNext::FContext& Context, UE::AnimNext::FWeakDecoratorPtr GraphInstancePtr, EAnimNextGraphSimulationSteps SimulationSteps) const
{
	if (RigVM && GraphInstancePtr.IsValid())
	{
		FRigVMExtendedExecuteContext RigVMExtendedExecuteContext;
		FAnimNextExecuteContext& AnimNextContext = RigVMExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		AnimNextContext.SetContextData(Context);
		AnimNextContext.InitializeWithGraph(SharedDataBuffer, GraphInstancePtr);
		AnimNextContext.SetSimulationSteps(SimulationSteps);

		RigVM->Execute(RigVMExtendedExecuteContext, TArray<TRigVMMemoryStorage*>(), FRigUnit_AnimNextGraphRoot::EventName);
	}
}

TArray<FRigVMExternalVariable> UAnimNextGraph::GetRigVMExternalVariables()
{
	return TArray<FRigVMExternalVariable>(); 
}

static TArray<UClass*> GetClassObjectsInPackage(UPackage* InPackage)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(InPackage, Objects, false);

	TArray<UClass*> ClassObjects;
	for (UObject* Object : Objects)
	{
		if (UClass* Class = Cast<UClass>(Object))
		{
			ClassObjects.Add(Class);
		}
	}

	return ClassObjects;
}

void UAnimNextGraph::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	// Whenever the asset is renamed/moved, generated classes parented to the old package
	// are not moved to the new package automatically (see FAssetRenameManager), so we
	// have to manually perform the move/rename, to avoid invalid reference to the old package

	// Note: while asset duplication doesn't duplicate the classes either, it is not a problem there
	// because we always recompile in post duplicate.
	TArray<UClass*> ClassObjects = GetClassObjectsInPackage(OldOuter->GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(ClassObject))
		{
			MemoryClass->Rename(nullptr, GetPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		}
	}
}

void UAnimNextGraph::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	TArray<UClass*> ClassObjects = GetClassObjectsInPackage(GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(ClassObject))
		{
			OutDeps.Add(MemoryClass);
		}
	}
}

void UAnimNextGraph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		int32 SharedDataArchiveBufferSize = 0;
		Ar << SharedDataArchiveBufferSize;

#if !WITH_EDITORONLY_DATA
		// When editor data isn't present, we don't persist the archive buffer as it is only needed on load
		// to populate the graph shared data
		TArray<uint8> SharedDataArchiveBuffer;
#endif

		SharedDataArchiveBuffer.SetNumUninitialized(SharedDataArchiveBufferSize);
		Ar.Serialize(SharedDataArchiveBuffer.GetData(), SharedDataArchiveBufferSize);

		LoadFromArchiveBuffer(SharedDataArchiveBuffer);
	}
	else if (Ar.IsSaving())
	{
#if WITH_EDITORONLY_DATA
		// We only save the archive buffer, if code changes we'll be able to de-serialize from it when
		// building the runtime buffer
		// This allows us to have editor only/non-shipping only properties that are stripped out on load
		int32 SharedDataArchiveBufferSize = SharedDataArchiveBuffer.Num();
		Ar << SharedDataArchiveBufferSize;
		Ar.Serialize(SharedDataArchiveBuffer.GetData(), SharedDataArchiveBufferSize);
#endif
	}
	else
	{
		// Counting, etc
		Ar << SharedDataBuffer;
		Ar << TrackedObjectsForGC;

#if WITH_EDITORONLY_DATA
		Ar << SharedDataArchiveBuffer;
#endif
	}
}

void UAnimNextGraph::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UAnimNextGraph* This = CastChecked<UAnimNextGraph>(InThis);
	Collector.AddStableReferenceArray(&This->TrackedObjectsForGC);

	Super::AddReferencedObjects(InThis, Collector);
}

bool UAnimNextGraph::LoadFromArchiveBuffer(const TArray<uint8>& InSharedDataArchiveBuffer)
{
	using namespace UE::AnimNext;

	// Reconstruct our graph shared data
	FMemoryReader GraphSharedDataArchive(InSharedDataArchiveBuffer);
	FDecoratorReader DecoratorReader(GraphSharedDataArchive);

	const FDecoratorReader::EErrorState ErrorState = DecoratorReader.ReadGraph(SharedDataBuffer, TrackedObjectsForGC);
	if (ErrorState == FDecoratorReader::EErrorState::None)
	{
		ResolvedRootDecoratorHandle = DecoratorReader.ResolveEntryPointHandle(RootDecoratorHandle);
		return true;
	}
	else
	{
		SharedDataBuffer.Empty(0);
		TrackedObjectsForGC.Empty(0);
		ResolvedRootDecoratorHandle = FAnimNextDecoratorHandle();
		return false;
	}
}
