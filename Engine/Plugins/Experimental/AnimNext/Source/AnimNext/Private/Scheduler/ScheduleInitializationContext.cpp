// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/ScheduleInitializationContext.h"
#include "Scheduler/ScheduleContext.h"

namespace UE::AnimNext
{

FScheduleInitializationContext::FScheduleInitializationContext(const FScheduleContext& InContext)
	: Context(InContext)
{
}

void FScheduleInitializationContext::ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, TUniquePtr<IParameterSource>&& InParameters) const
{
	FScheduleInstanceData& InstanceData = Context.GetInstanceData();
	InstanceData.ApplyParametersToScope(InScope, InOrdering, MoveTemp(InParameters));
}

}