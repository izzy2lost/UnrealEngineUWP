// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Trace.h"
#include "HAL/PlatformTime.h"

namespace UE::Trace { class FChannel; }

UE_TRACE_CHANNEL_EXTERN(MutableChannel, MUTABLERUNTIME_API)

/** Custom Mutable profiler scope. */
#define MUTABLE_CPUPROFILER_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(Mutable_##Name, MutableChannel)


/** Simple class that saves the time the scope is alive in the given location. */
class FMutableScopeTimer
{
public:
	FMutableScopeTimer(double& InResult) : Result(InResult)
	{
		StartTime = FPlatformTime::Seconds();
	}

	~FMutableScopeTimer()
	{
		Result =  FPlatformTime::Seconds() - StartTime;
	}
	
private:
	double StartTime = 0.0;
	double& Result;
};
