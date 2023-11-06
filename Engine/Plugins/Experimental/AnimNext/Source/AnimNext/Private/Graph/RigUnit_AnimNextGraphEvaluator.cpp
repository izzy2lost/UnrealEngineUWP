// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigUnit_AnimNextGraphEvaluator.h"
#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "EvaluationVM/EvaluationVM.h"
#include "Context.h"
#include "Graph/AnimNextGraph.h"
#include "Param/ParamStack.h"
#include "Graph/AnimNext_LODPose.h"
#include "AnimNextStats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextGraphEvaluator)

namespace UE::AnimNext::Private
{
	static TMap<uint32, FAnimNextGraphEvaluatorExecuteDefinition> GRegisteredGraphEvaluatorMethods;

	TArray<FRigVMFunctionArgument> GetGraphEvaluatorFunctionArguments(const FAnimNextGraphEvaluatorExecuteDefinition& ExecuteDefinition)
	{
		TArray<FRigVMFunctionArgument> Arguments;
		Arguments.Reserve(ExecuteDefinition.Arguments.Num());

		for (const FAnimNextGraphEvaluatorExecuteArgument& Argument : ExecuteDefinition.Arguments)
		{
			Arguments.Add(FRigVMFunctionArgument(Argument.Name, Argument.CPPType, ERigVMFunctionArgumentDirection::Input));
		}

		return Arguments;
	}
}

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
		FEvaluateTraversalContext TraversalContext;
		FEvaluationProgram EvaluationProgram = EvaluateGraph(Context, TraversalContext, GraphInstancePtr);

		if (!EvaluationProgram.IsEmpty())
		{
			FParamStack& ParamStack = FParamStack::Get();

			const UAnimNextGraph* Graph = VMExecuteContext.GetGraph();
			const FAnimNextGraphReferencePose* GraphReferencePosePtr = ParamStack.GetParamPtr<FAnimNextGraphReferencePose>(Graph->GetReferencePoseParam());
			const int32* GraphLODLevelPtr = ParamStack.GetParamPtr<int32>(Graph->GetCurrentLODParam());
			static FParamId ResultId("UE_Internal_ResultPose");
			FAnimNextGraphLODPose* ResultPosePtr = ParamStack.GetMutableParamPtr<FAnimNextGraphLODPose>(ResultId);
			static FParamId ExpectsAdditiveId("UE_Internal_GraphExpectsAdditive");
			const bool* bExpectsAdditivePtr = ParamStack.GetParamPtr<bool>(ExpectsAdditiveId);

			if(GraphReferencePosePtr && GraphLODLevelPtr && ResultPosePtr && bExpectsAdditivePtr)
			{
				FEvaluationVM EvaluationVM(EEvaluationFlags::All, *GraphReferencePosePtr->ReferencePose, *GraphLODLevelPtr);
				EvaluationProgram.Execute(EvaluationVM);

				TUniquePtr<FKeyframeState> EvaluatedKeyframe;
				if (EvaluationVM.PopValue(KEYFRAME_STACK_NAME, EvaluatedKeyframe))
				{
					ResultPosePtr->LODPose.CopyFrom(EvaluatedKeyframe->Pose);
				}
				else
				{
					// We need to output a valid pose, generate one
					
					FKeyframeState ReferenceKeyframe = EvaluationVM.MakeReferenceKeyframe(*bExpectsAdditivePtr);
					ResultPosePtr->LODPose.CopyFrom(ReferenceKeyframe.Pose);
				}
			}
		}
	}
}

void FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(const FAnimNextGraphEvaluatorExecuteDefinition& ExecuteDefinition)
{
	using namespace UE::AnimNext::Private;

	if (GRegisteredGraphEvaluatorMethods.Contains(ExecuteDefinition.Hash))
	{
		return;	// Already registered
	}

	GRegisteredGraphEvaluatorMethods.Add(ExecuteDefinition.Hash, ExecuteDefinition);

	const FString FullExecuteMethodName = FString::Printf(TEXT("FRigUnit_AnimNextGraphEvaluator::%s"), *ExecuteDefinition.MethodName);

	const TArray<FRigVMFunctionArgument> GraphEvaluatorArguments = GetGraphEvaluatorFunctionArguments(ExecuteDefinition);
	FRigVMRegistry::Get().Register(*FullExecuteMethodName, &FRigUnit_AnimNextGraphEvaluator::StaticExecute, FRigUnit_AnimNextGraphEvaluator::StaticStruct(), GraphEvaluatorArguments);
}

const FAnimNextGraphEvaluatorExecuteDefinition* FRigUnit_AnimNextGraphEvaluator::FindExecuteMethod(uint32 ExecuteMethodHash)
{
	using namespace UE::AnimNext::Private;

	return GRegisteredGraphEvaluatorMethods.Find(ExecuteMethodHash);
}
