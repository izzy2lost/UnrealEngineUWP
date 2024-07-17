// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOperatorCacheStatTracker.h"

#include "MetasoundOperatorCache.h"
#include "MetasoundGeneratorModule.h"
#include "ProfilingDebugging/CsvProfiler.h"

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
namespace Metasound
{
	CSV_DECLARE_CATEGORY_EXTERN(MetaSound_OperatorPool);
}

namespace Metasound::Engine
{
	CSV_DEFINE_CATEGORY(MetaSound_OperatorCacheUtilization, true);
	CSV_DEFINE_CATEGORY(MetaSound_AvailableCachedOperators, true);
	CSV_DEFINE_CATEGORY(Metasound_OperatorCacheMiss, true);

	namespace Private
	{
		static TAutoConsoleVariable<bool> CVarCacheMissCsvStatsEnabled(
			TEXT("au.MetaSound.OperatorPool.CacheMissCsvStatsEnabled"),
			true,
			TEXT("Record which metasounds incur a cache miss when building their graph.")
		);

		FString GetGraphName(FName GraphInstanceName)
		{
			return FPackageName::ObjectPathToPathWithinPackage(GraphInstanceName.ToString());
		}
	}

	FOperatorCacheStatTracker::FOperatorCacheStatTracker()
	{
#if CSV_PROFILER
		CsvEndFrameDelegateHandle = FCsvProfiler::Get()->OnCSVProfileEndFrame().AddRaw(this, &FOperatorCacheStatTracker::OnCsvProfileEndFrame);
#endif // CSV_PROFILER
	}

	FOperatorCacheStatTracker::~FOperatorCacheStatTracker()
	{
#if CSV_PROFILER
		FCsvProfiler::Get()->OnCSVProfileEndFrame().Remove(CsvEndFrameDelegateHandle);
		CsvEndFrameDelegateHandle.Reset();
#endif // CSV_PROFILER
	}

	void FOperatorCacheStatTracker::RecordPreCacheRequest(const FOperatorBuildData& BuildData, int32 NumIntancesToBuild)
	{
		if (BuildData.NumInstances <= 0)
		{
			return;
		}

		const FOperatorPoolEntryID EntryID{ BuildData.InitParams.Graph->GetInstanceID(), BuildData.InitParams.OperatorSettings };

		FScopeLock Lock(&CriticalSection);

		if (FStatEntry* Entry = StatEntries.Find(EntryID))
		{
			Entry->NumCacheSlots += NumIntancesToBuild;

			// Show how much we're increasing the existing cache for this sound by.
			UE_LOG(LogMetasoundGenerator, Log,
				TEXT("Pre-cached Metasound: %s [Graph: %s]. Added %d instances, Total: %d."),
				*BuildData.InitParams.MetaSoundName,
				*Entry->GraphName.ToString(),
				NumIntancesToBuild,
				Entry->NumCacheSlots);
		}
		else
		{
			// Get the asset name from the package path
			const FString GraphName = Private::GetGraphName(BuildData.InitParams.Graph->GetInstanceName());

			FStatEntry& StatEntry = StatEntries.Add(EntryID, FStatEntry
			{
				.GraphName = FName(*GraphName),
				.NumInstancesBuilt = NumIntancesToBuild,
				.NumCacheSlots = NumIntancesToBuild
			});

			if (GraphName == BuildData.InitParams.MetaSoundName)
			{
				UE_LOG(LogMetasoundGenerator, Log,
					TEXT("Pre-cached Metasound: %s. Requested: %d, Built: %d."),
					*BuildData.InitParams.MetaSoundName,
					BuildData.NumInstances,
					NumIntancesToBuild);
			}
			else
			{
				// Include the parent graph so it's clearer which this contributes to.
				UE_LOG(LogMetasoundGenerator, Log,
					TEXT("Pre-cached Metasound: %s [Graph: %s] Requested: %d, Built: %d."),
					*BuildData.InitParams.MetaSoundName,
					*StatEntry.GraphName.ToString(),
					BuildData.NumInstances,
					NumIntancesToBuild);
			}
		}
	}

	void FOperatorCacheStatTracker::RecordCacheEvent(const FOperatorPoolEntryID& OperatorID, bool bCacheHit, const FOperatorContext& Context)
	{
		if (!bCacheHit)
		{
#if CSV_PROFILER
			if (Private::CVarCacheMissCsvStatsEnabled->GetBool() &&
				Context.GraphInstanceName != NAME_Name)
			{
				const FString GraphName = Private::GetGraphName(Context.GraphInstanceName);
				FCsvProfiler::Get()->RecordCustomStat(*GraphName, CSV_CATEGORY_INDEX(Metasound_OperatorCacheMiss), 1, ECsvCustomStatOp::Accumulate);
			}
#endif // CSV_PROFILER
			return;
		}

		FScopeLock Lock(&CriticalSection);

		if (FStatEntry* StatEntry = StatEntries.Find(OperatorID))
		{
			--StatEntry->NumAvailableInCache;
			check(StatEntry->NumAvailableInCache >= 0);
		}

		--NumInCache;
		check(NumInCache >= 0);
	}

	void FOperatorCacheStatTracker::OnOperatorAdded(const FOperatorPoolEntryID& OperatorID)
	{
		FScopeLock Lock(&CriticalSection);

		if (FStatEntry* StatEntry = StatEntries.Find(OperatorID))
		{
			++StatEntry->NumAvailableInCache;
		}

		++NumInCache;
	}

	void FOperatorCacheStatTracker::OnOperatorTrimmed(const FOperatorPoolEntryID& OperatorID)
	{
		FScopeLock Lock(&CriticalSection);

		if (FStatEntry* StatEntry = StatEntries.Find(OperatorID))
		{
			--StatEntry->NumCacheSlots;
			--StatEntry->NumAvailableInCache;
			check(StatEntry->NumCacheSlots >= 0);
			check(StatEntry->NumAvailableInCache >= 0);

			if (StatEntry->NumCacheSlots == 0)
			{
				UE_LOG(LogMetasoundGenerator, Log, TEXT("Evicted %s from the Operator Pool."), *StatEntry->GraphName.ToString())
			}
			else
			{
				UE_LOG(LogMetasoundGenerator, Log,
					TEXT("Trimmed 1 instance of %s from the Operator Pool. %d instances remaining."),
					*StatEntry->GraphName.ToString(),
					StatEntry->NumCacheSlots);
			}
		}

		--NumInCache;
		check(NumInCache >= 0);
	}

	void FOperatorCacheStatTracker::OnOperatorRemoved(const FOperatorPoolEntryID& OperatorID)
	{
		FScopeLock Lock(&CriticalSection);

		if (FStatEntry* StatEntry = StatEntries.Find(OperatorID))
		{
			NumInCache -= StatEntry->NumAvailableInCache;
		}

		StatEntries.Remove(OperatorID);

		check(NumInCache >= 0);
	}

	void FOperatorCacheStatTracker::OnCsvProfileEndFrame()
	{
#if CSV_PROFILER
		QUICK_SCOPE_CYCLE_COUNTER(OperatorCacheStatTracker_RecordStats)

		FScopeLock Lock(&CriticalSection);

		CSV_CUSTOM_STAT(MetaSound_OperatorPool, TotalCachedOperators, NumInCache, ECsvCustomStatOp::Set);

		for (auto It = StatEntries.CreateIterator(); It; ++It)
		{
			const FOperatorPoolEntryID& PoolEntryID = It->Key;
			FStatEntry& Entry = It->Value;

			// Remove any nodes that have been evicted from the cache.
			if (Entry.NumCacheSlots <= 0)
			{
				It.RemoveCurrent();
				continue;
			}

			// Record cache utilization stats.
			{
				check(Entry.NumCacheSlots > 0);
				const int32 NumAvailableIncache = Entry.NumAvailableInCache;
				const int32 NumUsed = Entry.NumCacheSlots - NumAvailableIncache;
				const float UtilizationRatio = static_cast<float>(NumUsed) / static_cast<float>(Entry.NumCacheSlots);

				FCsvProfiler::Get()->RecordCustomStat(Entry.GraphName, CSV_CATEGORY_INDEX(MetaSound_AvailableCachedOperators), NumAvailableIncache, ECsvCustomStatOp::Set);
				FCsvProfiler::Get()->RecordCustomStat(Entry.GraphName, CSV_CATEGORY_INDEX(MetaSound_OperatorCacheUtilization), UtilizationRatio, ECsvCustomStatOp::Set);
			}
		}
#endif // CSV_PROFILER
	}
} // namespace Metasound::Private
#endif // METASOUND_OPERATORCACHEPROFILER_ENABLED
