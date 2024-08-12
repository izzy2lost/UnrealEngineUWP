// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphInstance.h"

#include "AnimNextStats.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "TraitCore/ExecutionContext.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "Logging/StructuredLog.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModuleInstance.h"

DEFINE_STAT(STAT_AnimNext_Graph_RigVM);

FAnimNextGraphInstance::FAnimNextGraphInstance()
{
#if WITH_EDITORONLY_DATA
	UAnimNextModule::OnModuleCompiled().AddRaw(this, &FAnimNextGraphInstance::OnModuleCompiled);
#endif
}

FAnimNextGraphInstance::~FAnimNextGraphInstance()
{
	Release();
}

void FAnimNextGraphInstance::Release()
{
#if WITH_EDITORONLY_DATA
	UAnimNextModule::OnModuleCompiled().RemoveAll(this);

	if(const UAnimNextAnimationGraph* Graph = GetAnimationGraph())
	{
		FScopeLock Lock(&Graph->GraphInstancesLock);
		Graph->GraphInstances.Remove(this);
	}
#endif

	if (!GraphInstancePtr.IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ModuleInstance = nullptr;
	ParentGraphInstance = nullptr;
	RootGraphInstance = nullptr;
	ExtendedExecuteContext.Reset();
	Components.Empty();
	AnimationGraph = nullptr;
}

bool FAnimNextGraphInstance::IsValid() const
{
	return GraphInstancePtr.IsValid();
}

const UAnimNextAnimationGraph* FAnimNextGraphInstance::GetAnimationGraph() const
{
	return AnimationGraph;
}

FName FAnimNextGraphInstance::GetEntryPoint() const
{
	return EntryPoint;
}

UE::AnimNext::FWeakTraitPtr FAnimNextGraphInstance::GetGraphRootPtr() const
{
	return GraphInstancePtr;
}

FAnimNextModuleInstance* FAnimNextGraphInstance::GetModuleInstance() const
{
	return ModuleInstance;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetParentGraphInstance() const
{
	return ParentGraphInstance;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetRootGraphInstance() const
{
	return RootGraphInstance;
}

bool FAnimNextGraphInstance::UsesAnimationGraph(const UAnimNextAnimationGraph* InAnimationGraph) const
{
	return AnimationGraph == InAnimationGraph;
}

bool FAnimNextGraphInstance::UsesEntryPoint(FName InEntryPoint) const
{
	if(AnimationGraph != nullptr)
	{
		if(InEntryPoint == NAME_None)
		{
			return EntryPoint == AnimationGraph->DefaultEntryPoint;
		}

		return InEntryPoint == EntryPoint;
	}
	return false;
}

bool FAnimNextGraphInstance::IsRoot() const
{
	return this == RootGraphInstance;
}

bool FAnimNextGraphInstance::HasUpdated() const
{
	return bHasUpdatedOnce;
}

void FAnimNextGraphInstance::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	if (!IsRoot())
	{
		return;	// If we aren't the root graph instance, we don't own the components
	}

	if (const UE::AnimNext::FGCGraphInstanceComponent* Component = TryGetComponent<UE::AnimNext::FGCGraphInstanceComponent>())
	{
		Component->AddReferencedObjects(Collector);
	}
}

UE::AnimNext::FGraphInstanceComponent* FAnimNextGraphInstance::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
{
	if (const TSharedPtr<UE::AnimNext::FGraphInstanceComponent>* Component = RootGraphInstance->Components.FindByHash(ComponentNameHash, ComponentName))
	{
		return Component->Get();
	}

	return nullptr;
}

UE::AnimNext::FGraphInstanceComponent& FAnimNextGraphInstance::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component)
{
	return *RootGraphInstance->Components.AddByHash(ComponentNameHash, ComponentName, MoveTemp(Component)).Get();
}

GraphInstanceComponentMapType::TConstIterator FAnimNextGraphInstance::GetComponentIterator() const
{
	return RootGraphInstance->Components.CreateConstIterator();
}

void FAnimNextGraphInstance::Update()
{
	bHasUpdatedOnce = true;
}

FRigVMExtendedExecuteContext& FAnimNextGraphInstance::GetExtendedExecuteContext()
{
	return ExtendedExecuteContext;
}

void FAnimNextGraphInstance::ExecuteLatentPins(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& LatentHandles, void* DestinationBasePtr, bool bIsFrozen)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_RigVM);

	if (!IsValid())
	{
		return;
	}

	if (URigVM* VM = AnimationGraph->VM)
	{
		FAnimNextExecuteContext& AnimNextContext = ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		AnimNextContext.SetContextData<FAnimNextGraphContextData>(ModuleInstance, this, LatentHandles, DestinationBasePtr, bIsFrozen);

		VM->ExecuteVM(ExtendedExecuteContext, FRigUnit_AnimNextShimRoot::EventName);

		// Reset the context to avoid issues if we forget to reset it the next time we use it
		AnimNextContext.DebugReset<FAnimNextGraphContextData>();
	}
}

#if WITH_EDITORONLY_DATA
void FAnimNextGraphInstance::Freeze()
{
	if (!IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ExtendedExecuteContext.Reset();
	Components.Empty();
	bHasUpdatedOnce = false;
}

void FAnimNextGraphInstance::Thaw()
{
	if (const UAnimNextAnimationGraph* AnimationGraphPtr = AnimationGraph)
	{
		Variables.MigrateToNewBagInstance(AnimationGraphPtr->VariableDefaults);

		ExtendedExecuteContext = AnimationGraphPtr->ExtendedExecuteContext;

		RebindPublicVariables();

		{
			UE::AnimNext::FExecutionContext Context(*this);
			if(const FAnimNextTraitHandle* FoundHandle = AnimationGraphPtr->ResolvedRootTraitHandles.Find(EntryPoint))
			{
				GraphInstancePtr = Context.AllocateNodeInstance(*this, *FoundHandle);
			}
		}

		if (!IsValid())
		{
			// We failed to allocate our instance, clear everything
			Release();
		}
	}
}

void FAnimNextGraphInstance::RebindPublicVariables()
{
	if(PublicVariablesState != EPublicVariablesState::Bound)
	{
		return;
	}

	// Setup external variables memory ptrs manually as we dont follow the pattern of owning multiple URigVMHosts like control rig.
	// InitializeVM() is called, but only sets up handles for the defaults in the module, not for an instance
	const int32 NumVariables = Variables.GetNumPropertiesInBag();
	TArray<FRigVMExternalVariableRuntimeData> ExternalVariableRuntimeData;
	ExternalVariableRuntimeData.Reserve(NumVariables);
	TConstArrayView<FPropertyBagPropertyDesc> Descs = Variables.GetPropertyBagStruct()->GetPropertyDescs();
	uint8* BasePtr = Variables.GetMutableValue().GetMemory();
	for(int32 VariableIndex = 0; VariableIndex < NumVariables; ++VariableIndex)
	{
		ExternalVariableRuntimeData.Emplace(Descs[VariableIndex].CachedProperty->ContainerPtrToValuePtr<uint8>(BasePtr));
	}
	ExtendedExecuteContext.ExternalVariableRuntimeData = MoveTemp(ExternalVariableRuntimeData);

	// Re-apply any bindings from our host
	for(const FCachedVariableBinding& CachedBinding : CachedVariableBindings)
	{
		// TODO: remove this linear search in FindPropertyDescByName with a hash table?
		if(const FPropertyBagPropertyDesc* Desc = Variables.FindPropertyDescByName(CachedBinding.VariableName))
		{
			int32 VariableIndex = Desc - Variables.GetPropertyBagStruct()->GetPropertyDescs().GetData();
			ExtendedExecuteContext.ExternalVariableRuntimeData[VariableIndex].Memory = CachedBinding.Memory;
		}
	}

	// Now reinitialize the 'instance', cache memory handles etc. in the context
	AnimationGraph->VM->InitializeInstance(ExtendedExecuteContext);
}

void FAnimNextGraphInstance::OnModuleCompiled(UAnimNextModule* InModule)
{
	// If we are hosted directly by a module, invalidate and mark our bindings as needing update 
	if(ModuleInstance && InModule == ModuleInstance->Module && ParentGraphInstance == nullptr)
	{
		UnbindPublicVariables();
	}
}

#endif

void FAnimNextGraphInstance::BindPublicVariables(TConstArrayView<FRigVMTraitScope> InTraitScopes)
{
	if(AnimationGraph == nullptr)
	{
		return;
	}

	if(PublicVariablesState == EPublicVariablesState::Bound)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	CachedVariableBindings.Reset();
#endif

	bool bPublicVariablesBound = false;
	for(const FRigVMTraitScope& TraitScope : InTraitScopes)
	{
		const FRigVMTrait_AnimNextPublicVariables* VariablesTrait = TraitScope.GetTrait<FRigVMTrait_AnimNextPublicVariables>();
		if(VariablesTrait == nullptr)
		{
			continue;
		}

		for(int32 TraitVariableIndex = 0; TraitVariableIndex < VariablesTrait->VariableNames.Num(); ++TraitVariableIndex)
		{
			FName VariableName = VariablesTrait->VariableNames[TraitVariableIndex];
			// TODO: remove this linear search in FindPropertyDescByName with a hash table?
			// TODO: multiple traits with the same name can collide at the moment, we should validate against this is at the compiler level to avoid issues
			const FPropertyBagPropertyDesc* Desc = Variables.FindPropertyDescByName(VariableName);
			if(Desc == nullptr)
			{
				continue;
			}

			if(!TraitScope.GetAdditionalMemoryHandles().IsValidIndex(TraitVariableIndex))
			{
				continue;
			}

			const FRigVMMemoryHandle& MemoryHandle = TraitScope.GetAdditionalMemoryHandles()[TraitVariableIndex];
			int32 VariableIndex = Desc - Variables.GetPropertyBagStruct()->GetPropertyDescs().GetData();
			if(Desc->CachedProperty->GetClass() != MemoryHandle.GetProperty()->GetClass())
			{
				UE_LOGFMT(LogAnimation, Warning, "Mismatched variable types when binding AnimNext graph: {Name}:{Type} vs {OtherType}", Desc->Name, Desc->CachedProperty->GetFName(), MemoryHandle.GetProperty()->GetFName());
				continue;
			}

			uint8* Memory = const_cast<uint8*>(MemoryHandle.GetData());
			ExtendedExecuteContext.ExternalVariableRuntimeData[VariableIndex].Memory = Memory;
#if WITH_EDITORONLY_DATA
			CachedVariableBindings.Add({ VariableName, Memory });
#endif
			bPublicVariablesBound = true;
		}
	}

	if(bPublicVariablesBound)
	{
		// Re-initialize memory handles
		AnimationGraph->VM->InitializeInstance(ExtendedExecuteContext, /* bCopyMemory = */false);
	}

	PublicVariablesState = EPublicVariablesState::Bound;
}

void FAnimNextGraphInstance::UnbindPublicVariables()
{
	if(PublicVariablesState != EPublicVariablesState::Bound)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	CachedVariableBindings.Reset();
#endif

	// Reset external variable ptrs to point to internal public vars
	const int32 NumVariables = Variables.GetNumPropertiesInBag();
	TConstArrayView<FPropertyBagPropertyDesc> Descs = Variables.GetPropertyBagStruct()->GetPropertyDescs();
	uint8* BasePtr = Variables.GetMutableValue().GetMemory();
	for(int32 VariableIndex = 0; VariableIndex < NumVariables; ++VariableIndex)
	{
		ExtendedExecuteContext.ExternalVariableRuntimeData[VariableIndex].Memory = Descs[VariableIndex].CachedProperty->ContainerPtrToValuePtr<uint8>(BasePtr);
	}

	// Re-initialize memory handles
	AnimationGraph->VM->InitializeInstance(ExtendedExecuteContext, /* bCopyMemory = */false);

	PublicVariablesState = EPublicVariablesState::Unbound;
}

