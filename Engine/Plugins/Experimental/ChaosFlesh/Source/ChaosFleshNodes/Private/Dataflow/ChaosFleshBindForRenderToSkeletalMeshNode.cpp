// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshBindForRenderToSkeletalMeshNode.h"

#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNodeFactory.h"


void FBindForRenderToSkeletalMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		//
		// @todo(dataflow) : Implemention binding for optimus deformation pipeline. 
		//

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}


