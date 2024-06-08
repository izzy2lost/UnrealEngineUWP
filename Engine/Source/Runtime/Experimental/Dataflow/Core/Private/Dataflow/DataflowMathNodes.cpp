// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowMathNodes.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowNodeColorsRegistry.h"

namespace Dataflow
{
	void RegisterDataflowMathNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathAbsNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathAddNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathCeilNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathCubeNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathDivideNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathExpNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathFloorNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathFracNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathInverseSquareRootNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathLogNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathLogXNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathMaximumNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathMinimumNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathMultiplyNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathNegateNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathPowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathReciprocalNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathRoundNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathSignNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathSquareNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathSquareRootNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathSubtractNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowMathTruncNode);

		// Math
		static constexpr FLinearColor CDefaultMathNodeBodyTintColor(0.f, 0.f, 0.f, 0.5f);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Math", FLinearColor(0.f, 0.4f, 0.8f), CDefaultMathNodeBodyTintColor);
	}
}

//-----------------------------------------------------------------------------------------------

FDataflowMathOneInputOperatorNode::FDataflowMathOneInputOperatorNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	:FDataflowNode(InParam, InGuid)
{
}

void FDataflowMathOneInputOperatorNode::RegisterInputsAndOutputs()
{
	RegisterInputConnection(&A);
	RegisterOutputConnection(&Result);

	// set the output to double for now so that it is strongly type and easy to connect to the next node
	// Cnce we can change the outpout type from the UI , this could be removed 
	SetOutputConcreteType(&Result, TDataflowSingleTypePolicy<double>::TypeName);
}

void FDataflowMathOneInputOperatorNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Result))
	{
		const double InA = GetValue(Context, &A);
		const double OutResult = ComputeResult(Context, InA);
		SetValue(Context, OutResult, &Result);
	}
}

//-----------------------------------------------------------------------------------------------

FDataflowMathTwoInputsOperatorNode::FDataflowMathTwoInputsOperatorNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	:FDataflowNode(InParam, InGuid)
{
}

void FDataflowMathTwoInputsOperatorNode::RegisterInputsAndOutputs()
{
	RegisterInputConnection(&A);
	RegisterInputConnection(&B);
	RegisterOutputConnection(&Result);

	// set the output to double for now so that it is strongly type and easy to connect to the next node
	// Cnce we can change the outpout type from the UI , this could be removed 
	SetOutputConcreteType(&Result, TDataflowSingleTypePolicy<double>::TypeName);
}

void FDataflowMathTwoInputsOperatorNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Result))
	{
		const double InA = GetValue(Context, &A);
		const double InB = GetValue(Context, &B);
		const double OutResult = ComputeResult(Context, InA, InB);
		SetValue(Context, OutResult, &Result);
	}
}

//-----------------------------------------------------------------------------------------------

FDataflowMathAddNode::FDataflowMathAddNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathAddNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	return (InA + InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathSubtractNode::FDataflowMathSubtractNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathSubtractNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	return (InA - InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathMultiplyNode::FDataflowMathMultiplyNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathMultiplyNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	return (InA * InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathDivideNode::FDataflowMathDivideNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
	RegisterInputConnection(&Fallback);
}

double FDataflowMathDivideNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	if (InB == 0)
	{
		return GetValue(Context, &Fallback);
	}
	return (InA / InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathMinimumNode::FDataflowMathMinimumNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathMinimumNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	return FMath::Min(InA, InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathMaximumNode::FDataflowMathMaximumNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathMaximumNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	return FMath::Max(InA, InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathReciprocalNode::FDataflowMathReciprocalNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
	RegisterInputConnection(&Fallback);
}

double FDataflowMathReciprocalNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	if (InA == 0)
	{
		return GetValue(Context, &Fallback);
	}
	return (1.0 / InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathSquareNode::FDataflowMathSquareNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathSquareNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return (InA * InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathCubeNode::FDataflowMathCubeNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathCubeNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return (InA * InA * InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathSquareRootNode::FDataflowMathSquareRootNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathSquareRootNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	if (InA < 0)
	{
		// Context.Error()
		return 0.0;
	}
	return FMath::Sqrt(InA * InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathInverseSquareRootNode::FDataflowMathInverseSquareRootNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
	RegisterInputConnection(&Fallback);
}

double FDataflowMathInverseSquareRootNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	if (InA == 0)
	{
		return GetValue(Context, &Fallback);
	}
	return FMath::InvSqrt(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathNegateNode::FDataflowMathNegateNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathNegateNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return -InA;
}

//-----------------------------------------------------------------------------------------------

FDataflowMathAbsNode::FDataflowMathAbsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathAbsNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::Abs(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathFloorNode::FDataflowMathFloorNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathFloorNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::FloorToDouble(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathCeilNode::FDataflowMathCeilNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathCeilNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::CeilToDouble(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathRoundNode::FDataflowMathRoundNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathRoundNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::RoundToDouble(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathTruncNode::FDataflowMathTruncNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathTruncNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::TruncToDouble(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathFracNode::FDataflowMathFracNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathFracNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::Frac(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathPowNode::FDataflowMathPowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathTwoInputsOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathPowNode::ComputeResult(Dataflow::FContext& Context, double InA, double InB) const
{
	return FMath::Pow(InA, InB);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathLogXNode::FDataflowMathLogXNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	Base.Value = 10.0; // default is base 10
	RegisterInputsAndOutputs();
	RegisterInputConnection(&Base);
}

double FDataflowMathLogXNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	const double InBase = GetValue(Context, &Base);
	if (InBase <= 0.f)
	{
		return 0.0;
	}
	return FMath::LogX(InBase, InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathLogNode::FDataflowMathLogNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathLogNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::Loge(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathExpNode::FDataflowMathExpNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathExpNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::Exp(InA);
}

//-----------------------------------------------------------------------------------------------

FDataflowMathSignNode::FDataflowMathSignNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowMathOneInputOperatorNode(InParam, InGuid)
{
	RegisterInputsAndOutputs();
}

double FDataflowMathSignNode::ComputeResult(Dataflow::FContext& Context, double InA) const
{
	return FMath::Sign(InA);
}