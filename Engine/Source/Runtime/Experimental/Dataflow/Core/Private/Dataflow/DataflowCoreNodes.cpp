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
		const FDataflowAnyType* SelectedInputReference = InCondition ? &TrueValue : &FalseValue;
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