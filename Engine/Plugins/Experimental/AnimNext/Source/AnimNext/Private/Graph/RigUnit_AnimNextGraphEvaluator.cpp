// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigUnit_AnimNextGraphEvaluator.h"
#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "Context.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextGraphEvaluator)

void FRigUnit_AnimNextGraphEvaluator::StaticExecute(FRigVMExtendedExecuteContext& RigVMExecuteContext, FRigVMMemoryHandleArray RigVMMemoryHandles, FRigVMPredicateBranchArray RigVMBranches)
{
	using namespace UE::AnimNext;

	const FAnimNextExecuteContext& VMExecuteContext = RigVMExecuteContext.GetPublicData<FAnimNextExecuteContext>();

	// Setup what we need to execute
	FExecutionContext Context(VMExecuteContext.GetSharedDataBuffer(), RigVMExecuteContext, RigVMMemoryHandles);
	FWeakDecoratorPtr GraphInstancePtr = VMExecuteContext.GetGraphInstancePtr();
	const EAnimNextGraphSimulationSteps SimulationSteps = VMExecuteContext.GetSimulationSteps();

	if (EnumHasAnyFlags(SimulationSteps, EAnimNextGraphSimulationSteps::Update))
	{
		const FContext& InterfaceContext = VMExecuteContext.GetContext();

		// Call pre/post update on our graph
		UpdateGraph(Context, GraphInstancePtr, InterfaceContext.GetDeltaTime());
	}

	if (EnumHasAnyFlags(SimulationSteps, EAnimNextGraphSimulationSteps::Evaluate))
	{
		// Call pre/post evaluate on our graph
		FPoseContainer PoseContainer;
		EvaluateGraph(Context, GraphInstancePtr, EEvaluationFlags::All, PoseContainer);

		// TODO: Write out our output pose/curves/attributes/etc
		//ExecuteContext_.GetContext().SetResult<FAnimationGraphNodeResult>(Result);
	}
}
