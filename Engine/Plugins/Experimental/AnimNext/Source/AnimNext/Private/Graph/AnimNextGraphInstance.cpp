// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphInstance.h"

#include "AnimNextStats.h"
#include "TraitCore/ExecutionContext.h"
#include "Module/AnimNextModule.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "Misc/ScopeRWLock.h"
#include "Param/IParameterSource.h"
#include "Param/ParametersProxy.h"

DEFINE_STAT(STAT_AnimNext_Graph_RigVM);

FAnimNextGraphInstance::FAnimNextGraphInstance() = default;

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
	ParentGraphInstance = nullptr;
	RootGraphInstance = nullptr;
	ExtendedExecuteContext.Reset();
	Components.Empty();
	Module = nullptr;
	GraphState = nullptr;
}

bool FAnimNextGraphInstance::IsValid() const
{
	return GraphInstancePtr.IsValid();
}

const UAnimNextModule* FAnimNextGraphInstance::GetModule() const
{
	return Module;
}

FName FAnimNextGraphInstance::GetEntryPoint() const
{
	return EntryPoint;
}

UE::AnimNext::FWeakTraitPtr FAnimNextGraphInstance::GetGraphRootPtr() const
{
	return GraphInstancePtr;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetParentGraphInstance() const
{
	return ParentGraphInstance;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetRootGraphInstance() const
{
	return RootGraphInstance;
}

bool FAnimNextGraphInstance::UsesModule(const UAnimNextModule* InModule) const
{
	return Module == InModule;
}

bool FAnimNextGraphInstance::UsesEntryPoint(FName InEntryPoint) const
{
	if(Module != nullptr)
	{
		if(InEntryPoint == NAME_None)
		{
			return EntryPoint == Module->DefaultEntryPoint;
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

	if(GraphState.IsValid())
	{
		GraphState->AddReferencedObjects(Collector);
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

void FAnimNextGraphInstance::ExecuteLatentPins(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& LatentHandles, void* DestinationBasePtr, bool bIsFrozen)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_RigVM);

	if (!IsValid())
	{
		return;
	}

	if (URigVM* VM = Module->VM)
	{
		FAnimNextExecuteContext& AnimNextContext = ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		AnimNextContext.SetContextData<FAnimNextGraphContextData>(this, LatentHandles, DestinationBasePtr, bIsFrozen);

		VM->ExecuteVM(ExtendedExecuteContext, FRigUnit_AnimNextShimRoot::EventName);

		// Reset the context to avoid issues if we forget to reset it the next time we use it
		AnimNextContext.DebugReset<FAnimNextGraphContextData>();
	}
}

void FAnimNextGraphInstance::Freeze()
{
	if (!IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	GraphState.Reset();
	ExtendedExecuteContext.Reset();
	Components.Empty();
	bHasUpdatedOnce = false;
}

void FAnimNextGraphInstance::Thaw()
{
	if (const UAnimNextModule* ModulePtr = Module)
	{
		GraphState = MakeUnique<UE::AnimNext::FParametersProxy>(Module);

		ExtendedExecuteContext.CopyMemoryStorage(ModulePtr->ExtendedExecuteContext);
		ModulePtr->VM->InitializeInstance(ExtendedExecuteContext);

		{
			UE::AnimNext::FExecutionContext Context(*this);
			if(const FAnimNextTraitHandle* FoundHandle = ModulePtr->ResolvedRootTraitHandles.Find(EntryPoint))
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

UE::AnimNext::FParamStack::FPushedLayerHandle FAnimNextGraphInstance::UpdateAndPushGraphState(float InDeltaTime) const
{
	if(GraphState.IsValid())
	{
		GraphState->Update(InDeltaTime);
		return UE::AnimNext::FParamStack::Get().PushLayer(GraphState->GetLayerHandle());
	}
	return UE::AnimNext::FParamStack::FPushedLayerHandle();
}

void FAnimNextGraphInstance::PopGraphState(UE::AnimNext::FParamStack::FPushedLayerHandle InHandle) const
{
	UE::AnimNext::FParamStack::Get().PopLayer(InHandle);
}