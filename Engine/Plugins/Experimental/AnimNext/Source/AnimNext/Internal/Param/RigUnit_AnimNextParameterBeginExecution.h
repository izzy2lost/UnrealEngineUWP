// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextParameterExecuteContext.h"
#include "RigUnit_AnimNextParameterBeginExecution.generated.h"

/**
 * Event for driving the skeleton hierarchy with variables and rig elements
 */
USTRUCT(meta=(DisplayName="Execute", Category="Events", NodeColor="1, 0, 0", Keywords="Begin,Update,Tick,Forward,Event"))
struct ANIMNEXT_API FRigUnit_AnimNextParameterBeginExecution : public FRigUnit_AnimNextParameterBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FAnimNextParameterExecuteContext ExecuteContext;

	static FName EventName;
};
