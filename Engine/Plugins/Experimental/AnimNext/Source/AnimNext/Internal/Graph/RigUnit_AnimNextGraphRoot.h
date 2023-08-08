// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"
#include "DecoratorBase/DecoratorHandle.h"

#include "RigUnit_AnimNextGraphRoot.generated.h"

/**
 * Animation graph output
 */
USTRUCT(meta=(DisplayName="Animation Output", Category="Events", NodeColor="1, 0, 0", Keywords="Root,Output"))
struct ANIMNEXT_API FRigUnit_AnimNextGraphRoot : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, Category = Result, meta = (Input))
	FAnimNextDecoratorHandle Result;

	// In order for this node to be considered an executable RigUnit, it needs a pin to derive from FRigVMExecuteContext
	// We keep it hidden it since we don't need it
	UPROPERTY()
	FAnimNextExecuteContext ExecuteContext;

	// This unit is our graph entry point, it needs an event so we can call it
	static FName EventName;
};
