// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SubGraphHost.h"

#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/PushReferenceKeyframe.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FSubGraphHostDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FSubGraphHostDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IDiscreteBlend)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IGarbageCollection)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IHierarchy)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
	DEFINE_ANIM_DECORATOR_END(FSubGraphHostDecorator)

	void FSubGraphHostDecorator::FInstanceData::Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		FDecorator::FInstanceData::Construct(Context, Binding);

		IGarbageCollection::RegisterWithGC(Context, Binding);
	}

	void FSubGraphHostDecorator::FInstanceData::Destruct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		FDecorator::FInstanceData::Destruct(Context, Binding);

		IGarbageCollection::UnregisterWithGC(Context, Binding);
	}

	uint32 FSubGraphHostDecorator::GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		return InstanceData->SubGraphSlots.Num();
	}

	void FSubGraphHostDecorator::GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		for (const FInstanceData::FSubGraphSlot& SubGraphEntry : InstanceData->SubGraphSlots)
		{
			Children.Add(SubGraphEntry.GraphInstance.GetGraphRootPtr());
		}
	}

	void FSubGraphHostDecorator::PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->SubGraphSlots.IsEmpty())
		{
			// We have no children, output a non-additive reference pose
			Context.AppendTask(FAnimNextPushReferenceKeyframeTask::MakeFromSkeleton());
		}
		else
		{
			// We only have one child or another decorator handles this, do nothing
		}
	}

	void FSubGraphHostDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TObjectPtr<const UAnimNextGraph> CurrentActiveSubGraph;
		if (InstanceData->CurrentlyActiveSubGraphIndex != INDEX_NONE)
		{
			CurrentActiveSubGraph = InstanceData->SubGraphSlots[InstanceData->CurrentlyActiveSubGraphIndex].SubGraph;
		}

		const TObjectPtr<const UAnimNextGraph> SubGraph = SharedData->GetSubGraph(Context, Binding);
		if (CurrentActiveSubGraph != SubGraph)
		{
			// We can't blend from a sub-graph to an empty pose right now
			check(SubGraph);

			// Find an empty slot we can use
			int32 FreeSlotIndex = INDEX_NONE;

			const int32 NumSubGraphSlots = InstanceData->SubGraphSlots.Num();
			for (int32 SlotIndex = 0; SlotIndex < NumSubGraphSlots; ++SlotIndex)
			{
				if (!InstanceData->SubGraphSlots[SlotIndex].GraphInstance.IsValid())
				{
					// This graph instance is invalid, we can re-use it
					FreeSlotIndex = SlotIndex;
					break;
				}
			}

			if (FreeSlotIndex == INDEX_NONE)
			{
				// All slots are in use, add a new one
				FreeSlotIndex = InstanceData->SubGraphSlots.AddDefaulted();
			}

			FInstanceData::FSubGraphSlot& SubGraphSlot = InstanceData->SubGraphSlots[FreeSlotIndex];
			SubGraphSlot.SubGraph = SubGraph;

			const int32 OldChildIndex = InstanceData->CurrentlyActiveSubGraphIndex;
			const int32 NewChildIndex = FreeSlotIndex;

			InstanceData->CurrentlyActiveSubGraphIndex = FreeSlotIndex;

			TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
			Context.GetInterface(Binding, DiscreteBlendDecorator);

			DiscreteBlendDecorator.OnBlendTransition(Context, OldChildIndex, NewChildIndex);
		}
	}

	void FSubGraphHostDecorator::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		const int32 NumSubGraphs = InstanceData->SubGraphSlots.Num();
		if (NumSubGraphs == 0)
		{
			return;
		}

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		for (int32 SubGraphIndex = 0; SubGraphIndex < NumSubGraphs; ++SubGraphIndex)
		{
			const float BlendWeight = DiscreteBlendDecorator.GetBlendWeight(Context, SubGraphIndex);

			FDecoratorUpdateState SubGraphDecoratorState = DecoratorState.WithWeight(BlendWeight);
			if (SubGraphIndex != InstanceData->CurrentlyActiveSubGraphIndex)
			{
				SubGraphDecoratorState = SubGraphDecoratorState.AsBlendingOut();
			}

			TraversalQueue.Push(InstanceData->SubGraphSlots[SubGraphIndex].GraphInstance.GetGraphRootPtr(), SubGraphDecoratorState);
		}
	}

	float FSubGraphHostDecorator::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (ChildIndex == InstanceData->CurrentlyActiveSubGraphIndex)
		{
			return 1.0f;	// Active child has full weight
		}
		else if (InstanceData->SubGraphSlots.IsValidIndex(ChildIndex))
		{
			return 0.0f;	// Other children have no weight
		}
		else
		{
			// Invalid child index
			return -1.0f;
		}
	}

	int32 FSubGraphHostDecorator::GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		return InstanceData->CurrentlyActiveSubGraphIndex;
	}

	void FSubGraphHostDecorator::OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		// We initiate immediately when we transition
		DiscreteBlendDecorator.OnBlendInitiated(Context, NewChildIndex);

		// We terminate immediately when we transition
		DiscreteBlendDecorator.OnBlendTerminated(Context, OldChildIndex);
	}

	void FSubGraphHostDecorator::OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->SubGraphSlots.IsValidIndex(ChildIndex))
		{
			// Allocate our new sub-graph instance
			FInstanceData::FSubGraphSlot& SubGraphEntry = InstanceData->SubGraphSlots[ChildIndex];
			SubGraphEntry.SubGraph->AllocateInstance(Context.GetGraphInstance(), SubGraphEntry.GraphInstance);
		}
	}

	void FSubGraphHostDecorator::OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->SubGraphSlots.IsValidIndex(ChildIndex))
		{
			// Deallocate our sub-graph instance
			InstanceData->SubGraphSlots[ChildIndex].GraphInstance.Release();
		}
	}

	void FSubGraphHostDecorator::AddReferencedObjects(const FExecutionContext& Context, const TDecoratorBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		for (FInstanceData::FSubGraphSlot& SubGraphEntry : InstanceData->SubGraphSlots)
		{
			Collector.AddPropertyReferencesWithStructARO(FAnimNextGraphInstancePtr::StaticStruct(), &SubGraphEntry.GraphInstance);
		}
	}
}
