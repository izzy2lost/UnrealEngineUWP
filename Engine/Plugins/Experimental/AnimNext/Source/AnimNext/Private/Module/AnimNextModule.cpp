// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Param/ParamTypeHandle.h"
#include "TraitCore/TraitReader.h"
#include "TraitCore/ExecutionContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Serialization/MemoryReader.h"
#include "AnimNextStats.h"
#include "RigVMRuntimeDataRegistry.h"
#include "Graph/AnimNextGraphEntryPoint.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Param/ParametersProxy.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectResource.h"

#if WITH_EDITOR	
#include "Engine/ExternalAssetDependencyGatherer.h"
REGISTER_ASSETDEPENDENCY_GATHERER(FExternalAssetDependencyGatherer, UAnimNextModule);
#endif // WITH_EDITOR

DEFINE_STAT(STAT_AnimNext_Graph_AllocateInstance);
DEFINE_STAT(STAT_AnimNext_Graph_UpdateParamLayer);

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextModule)

UAnimNextModule::UAnimNextModule(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExtendedExecuteContext.SetContextPublicDataStruct(FAnimNextExecuteContext::StaticStruct());
}

void UAnimNextModule::AllocateInstance(FAnimNextGraphInstancePtr& Instance, FName InEntryPoint) const
{
	AllocateInstanceImpl(nullptr, Instance, InEntryPoint);
}

void UAnimNextModule::AllocateInstance(FAnimNextGraphInstance& ParentGraphInstance, FAnimNextGraphInstancePtr& Instance, FName InEntryPoint) const
{
	AllocateInstanceImpl(&ParentGraphInstance, Instance, InEntryPoint);
}

void UAnimNextModule::AllocateInstanceImpl(FAnimNextGraphInstance* ParentGraphInstance, FAnimNextGraphInstancePtr& Instance, FName InEntryPoint) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_AllocateInstance);

	Instance.Release();

	const FName EntryPoint = (InEntryPoint == NAME_None) ? GetDefaultEntryPoint() : InEntryPoint;
	const FAnimNextTraitHandle ResolvedRootTraitHandle = ResolvedRootTraitHandles.FindRef(EntryPoint);
	if (!ResolvedRootTraitHandle.IsValid())
	{
		return;
	}

	{
		TUniquePtr<FAnimNextGraphInstance> InstanceImpl = MakeUnique<FAnimNextGraphInstance>();

		InstanceImpl->Module = this;
		InstanceImpl->ParentGraphInstance = ParentGraphInstance;
		InstanceImpl->EntryPoint = EntryPoint;
		InstanceImpl->GraphState = MakeUnique<UE::AnimNext::FParametersProxy>(this);

		// If we have a parent graph, use its root since we share the same root, otherwise if we have no parent, we are the root
		InstanceImpl->RootGraphInstance = ParentGraphInstance != nullptr ? ParentGraphInstance->GetRootGraphInstance() : InstanceImpl.Get();

		InstanceImpl->ExtendedExecuteContext.CopyMemoryStorage(ExtendedExecuteContext);
		VM->InitializeInstance(InstanceImpl->ExtendedExecuteContext);

		// Move our implementation so that we can use the instance to allocate our root node
		Instance.Impl = MoveTemp(InstanceImpl);
	}

	{
		UE::AnimNext::FExecutionContext Context(Instance);
		Instance.Impl->GraphInstancePtr = Context.AllocateNodeInstance(*Instance.Impl, ResolvedRootTraitHandle);
	}

	if (!Instance.IsValid())
	{
		// We failed to allocate our instance, clear everything
		Instance.Release();
	}

#if WITH_EDITORONLY_DATA
	if (Instance.IsValid())
	{
		FScopeLock Lock(&GraphInstancesLock);
		check(!GraphInstances.Contains(Instance.Impl.Get()));
		GraphInstances.Add(Instance.Impl.Get());
	}
#endif
}

void UAnimNextModule::UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle, float InDeltaTime) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_UpdateParamLayer);
	
	if (VM)
	{
		if (TSharedPtr<UE::AnimNext::FRigVMRuntimeData> RuntimeData = UE::AnimNext::FRigVMRuntimeDataRegistry::FindOrAddLocalRuntimeData(VM, GetRigVMExtendedExecuteContext()).Pin())
		{
			FRigVMExtendedExecuteContext& Context = RuntimeData->Context;

			check(Context.VMHash == VM->GetVMHash());

			FAnimNextExecuteContext& AnimNextContext = Context.GetPublicDataSafe<FAnimNextExecuteContext>();

			// Parameter setup
			AnimNextContext.SetContextData<FAnimNextParamContextData>(InHandle);

			// RigVM setup
			AnimNextContext.SetDeltaTime(InDeltaTime);

			VM->ExecuteVM(Context, FRigUnit_AnimNextBeginExecution::EventName);

			// Reset the context to avoid issues if we forget to reset it the next time we use it
			AnimNextContext.DebugReset<FAnimNextParamContextData>();
		}
	}
}

void UAnimNextModule::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		if(Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextCombineParameterBlocksAndGraphs)
		{
			// Skip over shared archive buffer if we are loading from an older version
			if (const FLinkerLoad* Linker = GetLinker())
			{
				const int32 LinkerIndex = GetLinkerIndex();
				const FObjectExport& Export = Linker->ExportMap[LinkerIndex];
				Ar.Seek(Export.SerialOffset + Export.SerialSize);
			}
		}
		else
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

		CacheDefaultEntryPoint();
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

void UAnimNextModule::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextGraphAccessSpecifiers)
	{
		DefaultState.State = PropertyBag_DEPRECATED;
	}
#endif
}

#if WITH_EDITOR
void UAnimNextModule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimNextModule, DefaultEntryPoint))
	{
		CacheDefaultEntryPoint(); 
	}
}
#endif

TConstArrayView<UE::AnimNext::FScheduleTerm> UAnimNextModule::GetTerms() const
{
	using namespace UE::AnimNext;

	static const FParamId OutputPoseId("UE_Internal_Graph_OutputPose");

	static const FScheduleTerm Terms[] =
	{
		FScheduleTerm(OutputPoseId, FAnimNextParamType::GetType<FAnimNextGraphLODPose>(), EScheduleTermDirection::Output)
	};

	return Terms;
}

bool UAnimNextModule::LoadFromArchiveBuffer(const TArray<uint8>& InSharedDataArchiveBuffer)
{
	using namespace UE::AnimNext;

	// Reconstruct our graph shared data
	FMemoryReader GraphSharedDataArchive(InSharedDataArchiveBuffer);
	FTraitReader TraitReader(GraphReferencedObjects, GraphSharedDataArchive);

	const FTraitReader::EErrorState ErrorState = TraitReader.ReadGraph(SharedDataBuffer);
	if (ErrorState == FTraitReader::EErrorState::None)
	{
		for(int32 EntryPointIndex = 0; EntryPointIndex < EntryPoints.Num(); ++EntryPointIndex)
		{
			const FAnimNextGraphEntryPoint& EntryPoint = EntryPoints[EntryPointIndex];
			ResolvedRootTraitHandles.Add(EntryPoint.EntryPointName, TraitReader.ResolveEntryPointHandle(EntryPoint.RootTraitHandle));
			ResolvedEntryPoints.Add(EntryPoint.EntryPointName, EntryPointIndex);
		}

		// Make sure our execute method is registered
		FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(ExecuteDefinition);
		return true;
	}
	else
	{
		SharedDataBuffer.Empty(0);
		ResolvedRootTraitHandles.Add(GetDefaultEntryPoint(), FAnimNextTraitHandle());
		return false;
	}
}

#if WITH_EDITORONLY_DATA
void UAnimNextModule::FreezeGraphInstances()
{
	FScopeLock Lock(&GraphInstancesLock);

	TSet<FAnimNextGraphInstance*> GraphInstancesCopy = GraphInstances;
	for (FAnimNextGraphInstance* GraphInstance : GraphInstancesCopy)
	{
		GraphInstance->Freeze();
	}
}

void UAnimNextModule::ThawGraphInstances()
{
	FScopeLock Lock(&GraphInstancesLock);

	TSet<FAnimNextGraphInstance*> GraphInstancesCopy = GraphInstances;
	for (FAnimNextGraphInstance* GraphInstance : GraphInstancesCopy)
	{
		GraphInstance->Thaw();
	}
}
#endif

void UAnimNextModule::SetDefaultEntryPoint(FName InEntryPoint)
{
	DefaultEntryPoint = InEntryPoint;

	CacheDefaultEntryPoint();
}

FName UAnimNextModule::GetDefaultEntryPoint() const
{
	if(CachedDefaultEntryPoint == NAME_None)
	{
		CacheDefaultEntryPoint();
	}

	return CachedDefaultEntryPoint;
}

void UAnimNextModule::CacheDefaultEntryPoint() const
{
	TStringBuilder<256> StringBuilder;
	StringBuilder.Append(GetPathName());
	StringBuilder.Append(TEXT(":"));
	DefaultEntryPoint.AppendString(StringBuilder);
	CachedDefaultEntryPoint = FName(StringBuilder.ToView());
}