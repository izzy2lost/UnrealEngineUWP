// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Param/ParamTypeHandle.h"
#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/ExecutionContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Serialization/MemoryReader.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_Graph_RigVM);
DEFINE_STAT(STAT_AnimNext_Graph_AllocateInstance);

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextGraph)

#if WITH_EDITORONLY_DATA
FAnimNextGraphInstance::FAnimNextGraphInstance(const FAnimNextGraphInstance& Other)
{
	*this = Other;
}

FAnimNextGraphInstance::FAnimNextGraphInstance(FAnimNextGraphInstance&& Other)
{
	*this = MoveTemp(Other);
}

FAnimNextGraphInstance& FAnimNextGraphInstance::operator=(const FAnimNextGraphInstance& Other)
{
	if (GraphInstancePtr.IsValid())
	{
		if (!Other.GraphInstancePtr.IsValid())
		{
			// We were valid but we won't be anymore, unregister ourself

			FRWScopeLock Lock(Graph->GraphInstancesLock, SLT_Write);

			check(Graph->GraphInstances.Contains(this));
			Graph->GraphInstances.Remove(this);
		}
		else
		{
			// Both instances remain valid, nothing to do
		}
	}
	else
	{
		if (Other.GraphInstancePtr.IsValid())
		{
			// We were invalid but we will become valid, register ourself

			FRWScopeLock Lock(Graph->GraphInstancesLock, SLT_Write);

			check(!Graph->GraphInstances.Contains(this));
			Graph->GraphInstances.Add(this);
		}
		else
		{
			// Both instances were invalid, nothing to do
		}
	}

	Graph = Other.Graph;
	GraphInstancePtr =  Other.GraphInstancePtr;
	ExtendedExecuteContext = Other.ExtendedExecuteContext;

	return *this;
}

FAnimNextGraphInstance& FAnimNextGraphInstance::operator=(FAnimNextGraphInstance&& Other)
{
	if (GraphInstancePtr.IsValid())
	{
		if (!Other.GraphInstancePtr.IsValid())
		{
			// We were valid but we won't be anymore, unregister ourself
			// Other will become valid, register it

			FRWScopeLock Lock(Graph->GraphInstancesLock, SLT_Write);

			check(Graph->GraphInstances.Contains(this));
			Graph->GraphInstances.Remove(this);

			check(!Graph->GraphInstances.Contains(&Other));
			Graph->GraphInstances.Add(&Other);
		}
		else
		{
			// Both instances remain valid, nothing to do
		}
	}
	else
	{
		if (Other.GraphInstancePtr.IsValid())
		{
			// We were invalid but we will become valid, register ourself
			// Other will become invalid, unregister it

			FRWScopeLock Lock(Graph->GraphInstancesLock, SLT_Write);

			check(!Graph->GraphInstances.Contains(this));
			Graph->GraphInstances.Add(this);

			check(Graph->GraphInstances.Contains(&Other));
			Graph->GraphInstances.Remove(&Other);
		}
		else
		{
			// Both instances were invalid, nothing to do
		}
	}

	Swap(Graph, Other.Graph);
	Swap(GraphInstancePtr, Other.GraphInstancePtr);
	Swap(ExtendedExecuteContext, Other.ExtendedExecuteContext);

	return *this;
}
#endif

FAnimNextGraphInstance::~FAnimNextGraphInstance()
{
	Release();
}

void FAnimNextGraphInstance::Release()
{
	if (!GraphInstancePtr.IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ExtendedExecuteContext.Reset();
	Components.Empty();

#if WITH_EDITORONLY_DATA
	FRWScopeLock Lock(Graph->GraphInstancesLock, SLT_Write);
	check(Graph->GraphInstances.Contains(this));
	Graph->GraphInstances.Remove(this);
#endif

	Graph = nullptr;
}

bool FAnimNextGraphInstance::IsValid() const
{
	return GraphInstancePtr.IsValid();
}

const UAnimNextGraph* FAnimNextGraphInstance::GetGraph() const
{
	return Graph;
}

UE::AnimNext::FWeakDecoratorPtr FAnimNextGraphInstance::GetGraphRootPtr() const
{
	return GraphInstancePtr;
}

bool FAnimNextGraphInstance::UsesGraph(const UAnimNextGraph* InGraph) const
{
	return Graph == InGraph;
}

void FAnimNextGraphInstance::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	if (const UE::AnimNext::FGCGraphInstanceComponent* Component = TryGetComponent<UE::AnimNext::FGCGraphInstanceComponent>())
	{
		Component->AddReferencedObjects(Collector);
	}
}

UE::AnimNext::FGraphInstanceComponent* FAnimNextGraphInstance::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
{
	if (const TSharedPtr<UE::AnimNext::FGraphInstanceComponent>* Component = Components.FindByHash(ComponentNameHash, ComponentName))
	{
		return Component->Get();
	}

	return nullptr;
}

UE::AnimNext::FGraphInstanceComponent& FAnimNextGraphInstance::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component)
{
	return *Components.AddByHash(ComponentNameHash, ComponentName, MoveTemp(Component)).Get();
}

GraphInstanceComponentMapType::TConstIterator FAnimNextGraphInstance::GetComponentIterator() const
{
	return Components.CreateConstIterator();
}

void FAnimNextGraphInstance::ExecuteLatentPin(int32 LatentPinIndex, void* DestinationPtr)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_RigVM);

	if (!IsValid())
	{
		return;
	}

	if (URigVM* VM = Graph->VM)
	{
		FAnimNextExecuteContext& AnimNextContext = ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		AnimNextContext.SetupForExecution(LatentPinIndex, DestinationPtr);

		VM->ExecuteVM(ExtendedExecuteContext, FRigUnit_AnimNextShimRoot::EventName);

		// Reset the context to avoid issues if we forget to reset it the next time we use it
		AnimNextContext.DebugReset();
	}
}

UAnimNextGraph::UAnimNextGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExtendedExecuteContext.SetContextPublicDataStruct(FAnimNextExecuteContext::StaticStruct());
	SetRigVMExtendedExecuteContext(&ExtendedExecuteContext);
}

void UAnimNextGraph::AllocateInstance(FAnimNextGraphInstance& Instance) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_AllocateInstance);

	Instance.Release();

	if (!ResolvedRootDecoratorHandle.IsValid())
	{
		return;
	}

	Instance.Graph = this;

	Instance.ExtendedExecuteContext.CopyMemoryStorage(ExtendedExecuteContext);
	VM->InitializeInstance(Instance.ExtendedExecuteContext);

	{
		UE::AnimNext::FExecutionContext Context(Instance);
		Instance.GraphInstancePtr = Context.AllocateNodeInstance(UE::AnimNext::FWeakDecoratorPtr(), ResolvedRootDecoratorHandle);
	}

	if (!Instance.GraphInstancePtr.IsValid())
	{
		// We failed to allocate our instance, clear everything
		Instance.Graph = nullptr;
		Instance.ExtendedExecuteContext.Reset();
		Instance.Components.Empty();
	}

#if WITH_EDITORONLY_DATA
	if (Instance.GraphInstancePtr.IsValid())
	{
		FRWScopeLock Lock(GraphInstancesLock, SLT_Write);
		check(!GraphInstances.Contains(&Instance));
		GraphInstances.Add(&Instance);
	}
#endif
}

TArray<FRigVMExternalVariable> UAnimNextGraph::GetRigVMExternalVariables()
{
	return TArray<FRigVMExternalVariable>(); 
}

void UAnimNextGraph::PostLoad()
{
	using namespace UE::AnimNext;
	
	Super::PostLoad();

	VM = RigVM;

	ExtendedExecuteContext.InvalidateCachedMemory();

	// In packaged builds, initialize the VM
	// In editor, the VM will be recompiled and initialized at RecompileVM
#if !WITH_EDITOR
	if (VM)
	{
		VM->ClearExternalVariables(ExtendedExecuteContext);
		VM->Initialize(ExtendedExecuteContext);
	}
#endif

	ReferencePoseId = FParamId(ReferencePose);
	CurrentLODId = FParamId(CurrentLOD);
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

		if (Ar.IsLoadingFromCookedPackage())
		{
			// If we are cooked, we populate our graph shared data otherwise in the editor we'll compile on load
			// and re-populate everything then to account for changes in code/content
			LoadFromArchiveBuffer(SharedDataArchiveBuffer);
		}
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

#if WITH_EDITORONLY_DATA
		Ar << SharedDataArchiveBuffer;
#endif
	}
}

TConstArrayView<UE::AnimNext::FScheduleTerm> UAnimNextGraph::GetTerms() const
{
	using namespace UE::AnimNext;

	static const FParamId OutputPoseId("UE_Internal_Graph_OutputPose");

	static const FScheduleTerm Terms[] =
	{
		FScheduleTerm(OutputPoseId, FAnimNextParamType::GetType<FAnimNextGraphLODPose>(), EScheduleTermDirection::Output)
	};

	return Terms;
}

bool UAnimNextGraph::LoadFromArchiveBuffer(const TArray<uint8>& InSharedDataArchiveBuffer)
{
	using namespace UE::AnimNext;

	// Reconstruct our graph shared data
	FMemoryReader GraphSharedDataArchive(InSharedDataArchiveBuffer);
	FDecoratorReader DecoratorReader(GraphReferencedObjects, GraphSharedDataArchive);

	const FDecoratorReader::EErrorState ErrorState = DecoratorReader.ReadGraph(SharedDataBuffer);
	if (ErrorState == FDecoratorReader::EErrorState::None)
	{
		ResolvedRootDecoratorHandle = DecoratorReader.ResolveEntryPointHandle(RootDecoratorHandle);

		// Make sure our execute method is registered
		FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(ExecuteDefinition);
		return true;
	}
	else
	{
		SharedDataBuffer.Empty(0);
		ResolvedRootDecoratorHandle = FAnimNextDecoratorHandle();
		return false;
	}
}

#if WITH_EDITORONLY_DATA
TArray<FAnimNextGraphInstance*> UAnimNextGraph::ReleaseAllInstances()
{
	// Make a copy of our live instances since we'll release them all
	TArray<FAnimNextGraphInstance*> TempGraphInstances;
	{
		// Lock shouldn't be necessary here since this should be called before graph compilation
		// If a graph is executing and we attempt to release it, things are likely to crash
		// Compilation must happen at a point in the frame where animation isn't executing
		FRWScopeLock Lock(GraphInstancesLock, SLT_Write);
		TempGraphInstances = GraphInstances.Array();
	}

	for (FAnimNextGraphInstance* GraphInstance : TempGraphInstances)
	{
		GraphInstance->Release();
	}

	return TempGraphInstances;
}
#endif
