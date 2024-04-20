// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshGenerateFiberDirectionsNode.h"

#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNodeFactory.h"


void FGenerateFiberDirectionsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		//
		// @todo(dataflow) : Implemention fiber direction
		//

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

