// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionExecutor.h"
#include "AvaTag.h"
#include "AvaTagHandleKeyFuncs.h"
#include "AvaTagList.h"
#include "AvaTransitionLayer.h"
#include "AvaTransitionSubsystem.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "Engine/World.h"
#include "Execution/AvaTransitionExecutorBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaTransitionExecutor, Log, All);

FAvaTransitionExecutor::FAvaTransitionExecutor(FAvaTransitionExecutorBuilder& InBuilder)
	: NullInstance(MoveTemp(InBuilder.NullInstance))
	, ContextName(MoveTemp(InBuilder.ContextName))
	, OnFinished(MoveTemp(InBuilder.OnFinished))
{
	// Add Exit then Enter instances to keep a consistent order of execution
	Instances.Reserve(InBuilder.ExitInstances.Num() + InBuilder.EnterInstances.Num());
	Instances.Append(MoveTemp(InBuilder.ExitInstances));
	Instances.Append(MoveTemp(InBuilder.EnterInstances));
}

FAvaTransitionExecutor::~FAvaTransitionExecutor()
{
	if (IsRunning())
	{
		// Log rather than ensuring because this can still happen when running behaviors and shutting down engine,
		// transitioning to another level, etc
		UE_LOG(LogAvaTransitionExecutor, Warning
			, TEXT("FAvaTransitionExecutor '%p' (in Context %s) has been destroyed while still running Behaviors!")
			, this
			, *ContextName);
	}
}

void FAvaTransitionExecutor::Setup()
{
	// Do a Setup pass on the Current Instances
	ForEachInstance(
		[this](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.SetLogContext(ContextName);
			InInstance.Setup();
		});

	struct FLayerInfo
	{
		// All the Behavior Instances found for a given Layer
		TArray<FAvaTransitionBehaviorInstance*> BehaviorInstances;

		// The accumulated Transition Type for a given Layer (e.g. combinations could be In, Out or In | Out)
		EAvaTransitionType TransitionType;
	};

	// Map of BehaviorInstance to the Resolved Tags
	TMap<FAvaTag, FLayerInfo> TagLayerInfo;
	{
		// Rough Estimate: assume each Instance is in its own single unique tag
		TagLayerInfo.Reserve(Instances.Num());

		// Gather the Layer info for each Instance
		for (FAvaTransitionBehaviorInstance& Instance : Instances)
		{
			for (const FAvaTag* Tag : Instance.GetTransitionLayer().GetTags())
			{
				FLayerInfo& LayerInfo = TagLayerInfo.FindOrAdd(*Tag);
				LayerInfo.BehaviorInstances.AddUnique(&Instance);
				LayerInfo.TransitionType |= Instance.GetTransitionType();
			}
		}
	}

	// Ensure there's an exiting null instance for every entering Transition Instance in a layer
	for (const TPair<FAvaTag, FLayerInfo>& Pair : TagLayerInfo)
	{
		const FLayerInfo& LayerInfo = Pair.Value;
		if (LayerInfo.TransitionType == EAvaTransitionType::In && !EnumHasAnyFlags(LayerInfo.TransitionType, EAvaTransitionType::Out))
		{
			check(!LayerInfo.BehaviorInstances.IsEmpty());
			FAvaTransitionBehaviorInstance& NullInstanceCopy = Instances.Add_GetRef(NullInstance);
			NullInstanceCopy.SetTransitionType(EAvaTransitionType::Out);
			NullInstanceCopy.SetOverrideLayer(LayerInfo.BehaviorInstances[0]->GetTransitionLayer());
			NullInstanceCopy.Setup();
		}
	}

	// For the Instances that are going out, if they belong in the same Transition Layer as an Instance going In
	// mark them as Needs Discard (this does not mean the scene will be discarded as there could be logic that reverts this flag)
	for (const TPair<FAvaTag, FLayerInfo>& Pair : TagLayerInfo)
	{
		if (!EnumHasAnyFlags(Pair.Value.TransitionType, EAvaTransitionType::In))
		{
			continue;
		}

		for (FAvaTransitionBehaviorInstance* Instance : Pair.Value.BehaviorInstances)
		{
			if (Instance->GetTransitionType() == EAvaTransitionType::In)
			{
				continue;
			}

			Instance->SetTransitionType(EAvaTransitionType::Out);

			if (FAvaTransitionScene* TransitionScene = Instance->GetTransitionContext().GetTransitionScene())
			{
				TransitionScene->SetFlags(EAvaTransitionSceneFlags::NeedsDiscard);
			}
		}
	}
}

void FAvaTransitionExecutor::Start()
{
	if (!ensureAlways(!IsRunning()))
	{
		UE_LOG(LogAvaTransitionExecutor, Error
			, TEXT("Trying to start an already-running FAvaTransitionExecutor '%p' (in Context %s)!")
			, this
			, *ContextName);
		return;
	}

	Setup();

	ForEachInstance(
		[](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.Start();
		});

	// All Behaviors might've finished on Start
	ConditionallyFinishBehaviors();
}

bool FAvaTransitionExecutor::IsRunning() const
{
	return Instances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance& InInstance)
		{
			return InInstance.IsRunning();
		});
}

TArray<const FAvaTransitionBehaviorInstance*> FAvaTransitionExecutor::GetBehaviorInstances(const FAvaTransitionLayerComparator& InComparator) const
{
	TArray<const FAvaTransitionBehaviorInstance*> OutInstances;
	OutInstances.Reserve(Instances.Num());

	ForEachInstance(
		[&OutInstances, &InComparator](const FAvaTransitionBehaviorInstance& InBehaviorInstance)
		{
			if (InComparator.Compare(InBehaviorInstance))
			{
				OutInstances.Add(&InBehaviorInstance);
			}
		});

	return OutInstances;
}

void FAvaTransitionExecutor::Stop()
{
	ForEachInstance(
		[](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.Stop();
		});

	ensureAlways(!IsRunning());
	ConditionallyFinishBehaviors();
}

TStatId FAvaTransitionExecutor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAvaTransitionExecutor, STATGROUP_Tickables);
}

void FAvaTransitionExecutor::Tick(float InDeltaSeconds)
{
	ForEachInstance(
		[InDeltaSeconds](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.Tick(InDeltaSeconds);
		});

	ConditionallyFinishBehaviors();
}

bool FAvaTransitionExecutor::IsTickable() const
{
	return IsRunning();
}

void FAvaTransitionExecutor::ForEachInstance(TFunctionRef<void(FAvaTransitionBehaviorInstance&)> InFunc)
{
	for (FAvaTransitionBehaviorInstance& Instance : Instances)
	{
		InFunc(Instance);
	}
}

void FAvaTransitionExecutor::ForEachInstance(TFunctionRef<void(const FAvaTransitionBehaviorInstance&)> InFunc) const
{
	for (const FAvaTransitionBehaviorInstance& Instance : Instances)
	{
		InFunc(Instance);
	}
}

void FAvaTransitionExecutor::ConditionallyFinishBehaviors()
{
	if (!IsRunning())
	{
		OnFinished.ExecuteIfBound();
	}
}
