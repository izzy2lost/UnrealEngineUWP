// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "StateTreeTaskBase.h"

#include "StateTreeRunParallelStateTreeTask.generated.h"

USTRUCT()
struct FStateTreeRunParallelStateTreeTaskInstanceData
{
	GENERATED_BODY()

	/** State tree and parameters that will be run when this task is started. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeReference StateTree;

	UPROPERTY(Transient)
	FStateTreeInstanceData TreeInstanceData;
};

/**
* Task that will run another state tree in the current state while allowing the current tree to continue selection and process of child state.
* It will succeed, fail or run depending on the result of the parallel tree.
* Less efficient then Linked Asset state, it has the advantage of allowing multiple trees to run in parallel.
*/
USTRUCT(meta = (DisplayName = "Run Parallel Tree", Category = "Common"))
struct FStateTreeRunParallelStateTreeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FStateTreeRunParallelStateTreeTaskInstanceData;
	
	FStateTreeRunParallelStateTreeTask();

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual void PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView) override;
#endif // WITH_EDITOR
};