// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/ScheduleTaskContext.h"

#include "Param/PropertyBagProxy.h"
#include "Scheduler/ScheduleContext.h"

namespace UE::AnimNext
{

FScheduleTaskContext::FScheduleTaskContext(const FScheduleContext& InContext)
	: Context(InContext)
{
}

void FScheduleTaskContext::ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, FName InInstanceId, FInstancedPropertyBag&& InPropertyBag) const
{
	TUniquePtr<FPropertyBagProxy> PropertyBagProxy = MakeUnique<FPropertyBagProxy>(InInstanceId, MoveTemp(InPropertyBag));
	Context.GetInstanceData().ApplyParametersToScope(InScope, InOrdering, MoveTemp(PropertyBagProxy));
}

}