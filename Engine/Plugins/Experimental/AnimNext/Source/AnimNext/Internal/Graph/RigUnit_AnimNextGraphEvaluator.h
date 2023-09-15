// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"

#include "RigUnit_AnimNextGraphEvaluator.generated.h"

/**
 * Animation graph evaluator
 * This node is only used at runtime.
 * It performs the animation graph update and evaluation through the data provided in the execution context.
 * It also holds all latent/lazy pins that the graph references in the editor.
 */
USTRUCT(meta=(DisplayName="Animation Runtime Output", Category="Events", NodeColor="1, 0, 0"))
struct ANIMNEXT_API FRigUnit_AnimNextGraphEvaluator : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, Output))
	FAnimNextExecuteContext ExecuteContext;

	static void StaticExecute(FRigVMExtendedExecuteContext& RigVMExecuteContext, FRigVMMemoryHandleArray RigVMMemoryHandles, FRigVMPredicateBranchArray RigVMBranches);
};
