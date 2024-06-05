// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowRenderingFactory.h"

#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Misc/MessageDialog.h"

namespace Dataflow
{
	FRenderingFactory* FRenderingFactory::Instance = nullptr;


	void FRenderingFactory::RenderNodeOutput(GeometryCollection::Facades::FRenderingFacade& RenderData, const FGraphRenderingState& State)
	{
		if (RenderMap.Contains(State.GetRenderKey()))
		{
			RenderMap[State.GetRenderKey()](RenderData, State);
		}
		else
		{
			UE_LOG(LogChaos, Warning,
				TEXT("Warning : Dataflow missing output renderer <%s,%s> for node %s"), 
				*State.GetRenderKey().Get<0>(),
				*State.GetRenderKey().Get<1>().ToString(),
				*State.GetNodeName().ToString());
		}
	}
}

