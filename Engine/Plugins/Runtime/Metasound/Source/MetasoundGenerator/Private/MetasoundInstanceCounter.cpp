// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundInstanceCounter.h"

namespace Metasound
{
	FConcurrentMetasoundInstanceCounter::FConcurrentMetasoundInstanceCounter(const FName& InName)
	: InstanceName(InName)
	{
		Increment();
	}

	// dtor
	FConcurrentMetasoundInstanceCounter::~FConcurrentMetasoundInstanceCounter()
	{
		Decrement();
	}

	void FConcurrentMetasoundInstanceCounter::Init(const FName& InName)
	{
		InstanceName = InName;
		Increment();
	}

	// static interface
	int64 FConcurrentMetasoundInstanceCounter::GetCountForName(const FName& InName)
	{
		FScopeLock Lock(&MapCritSec);
		if (FStats* Stats = StatsMap.Find(InName))
		{
			return Stats->GetCount();
		}

		return 0;
	}

	int64 FConcurrentMetasoundInstanceCounter::GetPeakCountForName(const FName& InName)
	{
		FScopeLock Lock(&MapCritSec);
		if (FStats* Stats = StatsMap.Find(InName))
		{
			return Stats->GetPeakCount();
		}

		return 0;
	}

	void FConcurrentMetasoundInstanceCounter::Increment()
	{
		FScopeLock Lock(&MapCritSec);
		GetOrAddStats().Increment();
	}

	void FConcurrentMetasoundInstanceCounter::Decrement()
	{
		FScopeLock Lock(&MapCritSec);
		GetOrAddStats().Decrement();
	}

	static const FString InstanceCounterCategory(TEXT("Metasound/Active_Generators"));
	FConcurrentMetasoundInstanceCounter::FStats& FConcurrentMetasoundInstanceCounter::GetOrAddStats()
	{
		FScopeLock Lock(&MapCritSec);

		// avoid re-constructing the trace counter string unless it's new
		if (StatsMap.Contains(InstanceName))
		{
			return StatsMap[InstanceName];
		}

#if COUNTERSTRACE_ENABLED
		return StatsMap.Emplace(InstanceName, FString::Printf(TEXT("%s - %s"), *InstanceCounterCategory, *InstanceName.ToString()));
#else
		return StatsMap.Add(InstanceName);
#endif
	}

	// ctor
#if COUNTERSTRACE_ENABLED
	FConcurrentMetasoundInstanceCounter::FStats::FStats(const FString& InName)
	: TraceCounter(MakeUnique<FCountersTrace::FCounterInt>(TraceCounterNameType_Dynamic, *InName, TraceCounterDisplayHint_None))
	{
	}
#endif

	// FConcurrentMetasoundInstanceCounter::FStats::

	void FConcurrentMetasoundInstanceCounter::FStats::Increment()
	{
		ensure(TraceCounter);
		TraceCounter->Increment();
		PeakCount = FMath::Max(PeakCount, GetCount());
	}


	void FConcurrentMetasoundInstanceCounter::FStats::Decrement()
	{
		ensure(TraceCounter);
		TraceCounter->Decrement();
	}

	int64 FConcurrentMetasoundInstanceCounter::FStats::GetCount()
	{
		ensure(TraceCounter);
#if COUNTERSTRACE_ENABLED
		return TraceCounter->Get();
#else
		return TraceCounter->GetValue();
#endif
	}

	int64 FConcurrentMetasoundInstanceCounter::FStats::GetPeakCount()
	{
		return PeakCount;
	}

} // namespace Metasound
