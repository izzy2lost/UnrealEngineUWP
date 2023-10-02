// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextSchedulePort.h"
#include "Scheduler/SchedulePortDefinition.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/ScheduleContext.h"
#include "Scheduler/SchedulePortAdapterContext.h"

void FAnimNextSchedulePortTask::RunPort(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	using namespace UE::AnimNext;

	if (const FSchedulePortDefinition* PortDefinition = FScheduler::FindPortDefinition(Name))
	{
		if (PortDefinition->Adapter)
		{
			if (const FAnimNextSchedulePort* PortParam = FParamStack::Get().GetParamPtr<FAnimNextSchedulePort>(Name))
			{
				FSchedulePortAdapterContext Context;
				Context.OutputObject = PortParam->Object.Get();
				Context.OutputData = PortParam->Data;
				PortDefinition->Adapter(Context);
			}
		}
	}
}