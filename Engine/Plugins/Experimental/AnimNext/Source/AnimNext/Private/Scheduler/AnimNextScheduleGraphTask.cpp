// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleGraphTask.h"
#include "Scheduler/ScheduleContext.h"
#include "Graph/AnimNextGraph.h"
#include "Context.h"
#include "Graph/AnimNextExecuteContext.h"

void FAnimNextScheduleGraphTask::RunGraph(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	using namespace UE::AnimNext;

	if (Graph)
	{
		FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();

		// Allocate our graph instance data
		if (!InstanceData.GraphInstanceData[TaskIndex].IsValid())
		{
			 Graph->AllocateInstance(InstanceData.GraphInstanceData[TaskIndex]);
		}

		// FIXME: Cant run concurrently so disabling for now.
		if(0)
		{
			const FContext Context;
			Graph->Run(Context, InstanceData.GraphInstanceData[TaskIndex], EAnimNextGraphSimulationSteps::All);
		}
	}
}
