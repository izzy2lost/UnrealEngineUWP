// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleContext.h"
#include "Scheduler/AnimNextSchedule.h"

namespace UE::AnimNext
{

// The currently attached schedule context for this thread
static thread_local const FScheduleContext* GScheduleContext = nullptr;

void FScheduleContext::AttachToCurrentThread(const FScheduleContext& InContext)
{
	check(GScheduleContext == nullptr);
	GScheduleContext = &InContext;
}

void FScheduleContext::DetachFromCurrentThread()
{
	check(GScheduleContext != nullptr);
	GScheduleContext = nullptr;
}

const FScheduleContext& FScheduleContext::Get()
{
	check(GScheduleContext != nullptr);
	return *GScheduleContext;
}

}
