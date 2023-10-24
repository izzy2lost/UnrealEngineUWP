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
#include "Serialization/MemoryReader.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_Graph);
DEFINE_STAT(STAT_AnimNext_Graph_AllocateInstance);

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextGraph)

namespace UE::AnimNext::Graph
{
const UE::AnimNext::FParamId OutputPoseId("UE_Internal_Graph_OutputPose");
}

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

	// We need an execution context for this graph to be active when we delete the graph instance
	UE::AnimNext::FExecutionContext Context(Graph->SharedDataBuffer);

	GraphInstancePtr.Reset();
	Graph = nullptr;
	ExtendedExecuteContext.Reset();
}

bool FAnimNextGraphInstance::IsValid() const
{
	return GraphInstancePtr.IsValid();
}

UAnimNextGraph::UAnimNextGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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

	UE::AnimNext::FExecutionContext Context(SharedDataBuffer);
	Instance.GraphInstancePtr = Context.AllocateNodeInstance(UE::AnimNext::FWeakDecoratorPtr(), ResolvedRootDecoratorHandle);

	Instance.ExtendedExecuteContext.CopyMemoryStorage(ExtendedExecuteContext);

	VM->InitializeInstance(Instance.ExtendedExecuteContext);
}

void UAnimNextGraph::Run(const UE::AnimNext::FContext& Context, FAnimNextGraphInstance& GraphInstance, EAnimNextGraphSimulationSteps SimulationSteps) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph);

	if (VM && GraphInstance.IsValid())
	{
		FAnimNextExecuteContext& AnimNextContext = GraphInstance.ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		AnimNextContext.SetContextData(Context);
		AnimNextContext.InitializeWithGraph(this, SharedDataBuffer, GraphInstance.GraphInstancePtr);
		AnimNextContext.SetSimulationSteps(SimulationSteps);

		VM->ExecuteVM(GraphInstance.ExtendedExecuteContext, FRigUnit_AnimNextShimRoot::EventName);

		// Reset the context to avoid issues if we forget to reset it the next time we use it
		AnimNextContext.DebugReset();
	}
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
	DeltaTimeId = FParamId(DeltaTime);
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

	static const FScheduleTerm Terms[] =
	{
		FScheduleTerm(Graph::OutputPoseId, FAnimNextParamType::GetType<FAnimNextGraphLODPose>(), EScheduleTermDirection::Output)
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
