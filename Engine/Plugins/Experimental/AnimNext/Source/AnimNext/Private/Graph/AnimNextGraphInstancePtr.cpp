// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphInstancePtr.h"

#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Param/IParameterSource.h"

FAnimNextGraphInstancePtr::FAnimNextGraphInstancePtr() = default;
FAnimNextGraphInstancePtr::FAnimNextGraphInstancePtr(FAnimNextGraphInstancePtr&&) = default;
FAnimNextGraphInstancePtr& FAnimNextGraphInstancePtr::operator=(FAnimNextGraphInstancePtr&&) = default;

FAnimNextGraphInstancePtr::~FAnimNextGraphInstancePtr()
{
	Release();
}

void FAnimNextGraphInstancePtr::Release()
{
	if (Impl)
	{
#if WITH_EDITORONLY_DATA
		{
			const UAnimNextGraph* Graph = Impl->GetGraph();
			FScopeLock Lock(&Graph->GraphInstancesLock);
			Graph->GraphInstances.Remove(Impl.Get());
		}
#endif

		// Destroy the graph instance
		Impl->Release();

		// Reset our unique ptr
		Impl.Reset();
	}
}

bool FAnimNextGraphInstancePtr::IsValid() const
{
	return Impl && Impl->IsValid();
}

const UAnimNextGraph* FAnimNextGraphInstancePtr::GetGraph() const
{
	return Impl ? Impl->GetGraph() : nullptr;
}

UE::AnimNext::FWeakTraitPtr FAnimNextGraphInstancePtr::GetGraphRootPtr() const
{
	return Impl ? Impl->GetGraphRootPtr() : UE::AnimNext::FWeakTraitPtr();
}

FAnimNextGraphInstance* FAnimNextGraphInstancePtr::GetImpl() const
{
	return Impl.Get();
}

bool FAnimNextGraphInstancePtr::UsesGraph(const UAnimNextGraph* InGraph) const
{
	return Impl ? Impl->UsesGraph(InGraph) : false;
}

bool FAnimNextGraphInstancePtr::IsRoot() const
{
	return Impl ? Impl->IsRoot() : true;
}

bool FAnimNextGraphInstancePtr::HasUpdated() const
{
	return Impl ? Impl->HasUpdated() : false;
}

void FAnimNextGraphInstancePtr::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	if (Impl)
	{
		Impl->AddStructReferencedObjects(Collector);
	}
}

UE::AnimNext::FGraphInstanceComponent* FAnimNextGraphInstancePtr::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
{
	return Impl ? Impl->TryGetComponent(ComponentNameHash, ComponentName) : nullptr;
}

UE::AnimNext::FGraphInstanceComponent& FAnimNextGraphInstancePtr::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component)
{
	check(Impl);
	return Impl->AddComponent(ComponentNameHash, ComponentName, MoveTemp(Component));
}

GraphInstanceComponentMapType::TConstIterator FAnimNextGraphInstancePtr::GetComponentIterator() const
{
	check(Impl);
	return Impl->GetComponentIterator();
}

void FAnimNextGraphInstancePtr::Update()
{
	check(Impl);
	Impl->Update();
}

UE::AnimNext::FParamStack::FPushedLayerHandle FAnimNextGraphInstancePtr::UpdateAndPushGraphState(float InDeltaTime) const
{
	check(Impl);
	return Impl->UpdateAndPushGraphState(InDeltaTime);
}

void FAnimNextGraphInstancePtr::PopGraphState(UE::AnimNext::FParamStack::FPushedLayerHandle InHandle) const
{
	check(Impl);
	Impl->PopGraphState(InHandle);
}
