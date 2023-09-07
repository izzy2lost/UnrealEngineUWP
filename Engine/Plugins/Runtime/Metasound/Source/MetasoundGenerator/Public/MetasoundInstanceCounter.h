// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformAtomics.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

namespace Metasound
{

#if COUNTERSTRACE_ENABLED
	using CounterType = FCountersTrace::FCounterInt;
#else
	using CounterType = FThreadSafeCounter64;
#endif

	class FConcurrentMetasoundInstanceCounter
	{
	public:
		// ctor
		FConcurrentMetasoundInstanceCounter() = default;
		FConcurrentMetasoundInstanceCounter(const FName& InName);
		FConcurrentMetasoundInstanceCounter(const FString& InName);

		// dtor
		virtual ~FConcurrentMetasoundInstanceCounter();

		// for non-RAII clients
		void Init(const FName& InName);
		void Init(const FString& InName);

		// pure virtual interface
		virtual const FName& GetCategoryName() const = 0;

		// static interface
		static int64 GetCountForName(const FName& InName);
		static int64 GetPeakCountForName(const FName& InName);


	private:
		struct FStats
		{
		public:
			// ctor
#if COUNTERSTRACE_ENABLED
			FStats(const FString& InName);
#else
			FStats() = default;
#endif

			void Increment();
			void Decrement();

			int64 GetCount();
			int64 GetPeakCount();

		private:
			TUniquePtr<CounterType> TraceCounter;
			int64 PeakCount;

		}; // struct FStats

	// non-static data
		FName InstanceName;

		void Increment();
		void Decrement();
		FStats& GetOrAddStats();


	// static data
		inline static TMap<FName, FStats> StatsMap;
		inline static FCriticalSection MapCritSec;
	}; // class FConcurrentMetasoundInstanceCounter
} // namespace Metasound
