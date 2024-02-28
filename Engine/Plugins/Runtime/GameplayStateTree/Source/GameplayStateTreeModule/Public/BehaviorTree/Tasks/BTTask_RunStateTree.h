// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"

#include "BTTask_RunStateTree.generated.h"

/**
 * RunStateTree task allows the execution of state tree with the StateTreeComponentSchema inside a behavior tree.
 */
 UCLASS(MinimalAPI)
class UBTTask_RunStateTree : public UBTTaskNode
{
	GENERATED_BODY()
public:
	GAMEPLAYSTATETREEMODULE_API UBTTask_RunStateTree(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	GAMEPLAYSTATETREEMODULE_API virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	GAMEPLAYSTATETREEMODULE_API virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	GAMEPLAYSTATETREEMODULE_API virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	
	/** State tree that will be run when the task is selected. */
	UPROPERTY(EditAnywhere, Category = Task, meta = (Schema = "/Script/GameplayStateTreeModule.StateTreeComponentSchema"))
	FStateTreeReference StateTreeRef;

	UPROPERTY(Transient)
	FStateTreeInstanceData InstanceData;

	/** Interval between state tree update. */
	UPROPERTY(EditAnywhere, Category = Task, meta = (ClampMin = "0.001"))
	float Interval = 0.01f;

	/** Random deviation on the interval between each state tree update. */
	UPROPERTY(EditAnywhere, Category = Task, meta = (ClampMin = "0.0"))
	float RandomDeviation = 0.0f;
};
