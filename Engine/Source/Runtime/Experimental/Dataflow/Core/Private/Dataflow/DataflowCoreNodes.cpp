// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCoreNodes.h"



FDataflowReRouteNode::FDataflowReRouteNode(const Dataflow::FNodeParameters& Param, FGuid InGuid)
	: Super(Param, InGuid)
{
	RegisterInputConnection(&Value);
	RegisterOutputConnection(&Value, &Value);
}

void FDataflowReRouteNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	ForwardInput(Context, &Value, &Value);
}

bool FDataflowReRouteNode::OnInputTypeChanged(const FDataflowInput* Input)
{
	return SetOutputConcreteType(&Value, Input->GetType());
}

bool FDataflowReRouteNode::OnOutputTypeChanged(const FDataflowOutput* Input)
{
	return SetInputConcreteType(&Value, Input->GetType());
}

FDataflowBranchNode::FDataflowBranchNode(const Dataflow::FNodeParameters& Param, FGuid InGuid)
	: Super(Param, InGuid)
{
	RegisterInputConnection(&TrueValue);
	RegisterInputConnection(&FalseValue);
	RegisterInputConnection(&bCondition);
	RegisterOutputConnection(&Result);
}

void FDataflowBranchNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Result))
	{
		const bool InCondition = GetValue<bool>(Context, &bCondition);
		const void* SelectedInputReference = InCondition ? &TrueValue : &FalseValue;
		if (IsConnected(SelectedInputReference))
		{
			ForwardInput(Context, SelectedInputReference, &Result);
		}
		else
		{
			// TODO : throw an invalid type error when context error is available 
			// Context.Error(TEXT("Both True and False Inputs must be connected"));
		}
	}
}

bool FDataflowBranchNode::OnInputTypeChanged(const FDataflowInput* Input)
{
	// using single bitwise | operator to avoid skipping calls to SetInputConcreteType because of the shortcircuiting of ||
	// need to disable the warning for static analysis ( V792 warning )
	return bool(
		SetInputConcreteType(&TrueValue, Input->GetType())
	  | SetInputConcreteType(&FalseValue, Input->GetType()) //-V792
	  | SetOutputConcreteType(&Result, Input->GetType()) //-V792
		);
}

bool FDataflowBranchNode::OnOutputTypeChanged(const FDataflowOutput* Input)
{
	// using single bitwise | operator to avoid skipping calls to SetInputConcreteType because of the shortcircuiting of ||
	// need to disable the warning for static analysis ( V792 warning )
	return bool(
		  SetInputConcreteType(&TrueValue, Input->GetType())
		| SetInputConcreteType(&FalseValue, Input->GetType()) //-V792
		);
}