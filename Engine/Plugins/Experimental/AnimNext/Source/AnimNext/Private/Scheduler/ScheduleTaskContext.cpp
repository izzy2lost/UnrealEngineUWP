// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/ScheduleTaskContext.h"
#include "Scheduler/ScheduleContext.h"

namespace UE::AnimNext
{

FScheduleTaskContext::FScheduleTaskContext(const FScheduleContext& InContext)
	: Context(InContext)
{
}

void FScheduleTaskContext::ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, TUniquePtr<IParameterSource>&& InParameters) const
{
	FScheduleInstanceData& InstanceData = Context.GetInstanceData();
	InstanceData.ApplyParametersToScope(InScope, InOrdering, MoveTemp(InParameters));
}

}