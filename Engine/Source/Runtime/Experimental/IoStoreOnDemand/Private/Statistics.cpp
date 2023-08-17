// Copyright Epic Games, Inc. All Rights Reserved.

#include "Statistics.h"

#include "AnalyticsEventAttribute.h"
#include "Misc/CoreDelegates.h"

LLM_DEFINE_TAG(Ias);

#if IAS_WITH_STATISTICS

namespace UE::IO::Private
{

////////////////////////////////////////////////////////////////////////////////
static int32 BytesToApproxMB(uint64 Bytes) { return int32(Bytes >> 20); }
static int32 BytesToApproxKB(uint64 Bytes) { return int32(Bytes >> 10); }

/**
 * Code taken from SummarizeTraceCommandlet.cpp pending discussion on moving it
 * somewhere for general use.
 * Currently not thread safe!
 */
class FIncrementalVariance
{
public:
	FIncrementalVariance()
		: Count(0)
		, Mean(0.0)
		, VarianceAccumulator(0.0)
	{

	}

	uint64 GetCount() const
	{
		return Count;
	}

	double GetMean() const
	{
		return Mean;
	}

	/**
	* Compute the variance given Welford's accumulator and the overall count
	*
	* @return The variance in sample units squared
	*/
	double GetVariance() const
	{
		double Result = 0.0;

		if (Count > 1)
		{
			// Welford's final step, dependent on sample count
			Result = VarianceAccumulator / double(Count - 1);
		}

		return Result;
	}

	/**
	* Compute the standard deviation given Welford's accumulator and the overall count
	*
	* @return The standard deviation in sample units
	*/
	double GetDeviation() const
	{
		double Result = 0.0;

		if (Count > 1)
		{
			// Welford's final step, dependent on sample count
			double DeviationSqrd = VarianceAccumulator / double(Count - 1);

			// stddev is sqrt of variance, to restore to units (vs. units squared)
			Result = sqrt(DeviationSqrd);
		}

		return Result;
	}

	/**
	* Perform an increment of work for Welford's variance, from which we can compute variation and standard deviation
	*
	* @param InSample	The new sample value to operate on
	*/
	void Increment(const double InSample)
	{
		Count++;
		const double OldMean = Mean;
		Mean += ((InSample - Mean) / double(Count));
		VarianceAccumulator += ((InSample - Mean) * (InSample - OldMean));
	}

	/**
	* Merge with another IncrementalVariance series in progress
	*
	* @param Other	The other variance incremented from another mutually exclusive population of analogous data.
	*/
	void Merge(const FIncrementalVariance& Other)
	{
		// empty other, nothing to do
		if (Other.Count == 0)
		{
			return;
		}

		// empty this, just copy other
		if (Count == 0)
		{
			Count = Other.Count;
			Mean = Other.Mean;
			VarianceAccumulator = Other.VarianceAccumulator;
			return;
		}

		const double TotalPopulation = static_cast<double>(Count + Other.Count);
		const double MeanDifference = Mean - Other.Mean;
		const double A = ((Count - 1) * GetVariance()) + ((Other.Count - 1) * Other.GetVariance());
		const double B = (MeanDifference) * (MeanDifference) * (Count * Other.Count / TotalPopulation);
		const double MergedVariance = (A + B) / (TotalPopulation - 1);

		const uint64 NewCount = Count + Other.Count;
		const double NewMean = ((Mean * double(Count)) + (Other.Mean * double(Other.Count))) / double(NewCount);
		const double NewVarianceAccumulator = MergedVariance * (NewCount - 1);

		Count = NewCount;
		Mean = NewMean;
		VarianceAccumulator = NewVarianceAccumulator;
	}

	/**
	* Reset state back to initialized.
	*/
	void Reset()
	{
		Count = 0;
		Mean = 0.0;
		VarianceAccumulator = 0.0;
	}

private:
	uint64 Count;
	double Mean;
	double VarianceAccumulator;
};

////////////////////////////////////////////////////////////////////////////////
// TRACE STATS

#if COUNTERSTRACE_ENABLED
	using FCounterInt		= FCountersTrace::FCounterInt;
	using FCounterAtomicInt = FCountersTrace::FCounterAtomicInt;
#else
	template <typename Type>
	struct TCounterInt
	{
		TCounterInt(...)  {}
		void Set(int64 i) { V = i; }
		void Add(int64 d) { V += d; }
		int64 Get() const { return V;}
		Type V = 0;
	};
	using FCounterInt		= TCounterInt<int64>;
	using FCounterAtomicInt = TCounterInt<std::atomic<int64>>;
#endif 

// iorequest stats
FCounterInt				GIoRequestCount(TEXT("Ias/IoRequestCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestReadCount(TEXT("Ias/IoRequestReadCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestReadBytes(TEXT("Ias/IoRequestReadBytes"), TraceCounterDisplayHint_Memory);
FCounterInt				GIoRequestCancelCount(TEXT("Ias/IoRequestCancelCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestErrorCount(TEXT("Ias/IoRequestErrorCount"), TraceCounterDisplayHint_None);
// cache stats
FCounterAtomicInt		GCacheErrorCount(TEXT("Ias/CacheErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheGetCount(TEXT("Ias/CacheGetCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutCount(TEXT("Ias/CachePutCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutExistingCount(TEXT("Ias/CachePutExistingCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutRejectCount(TEXT("Ias/CachePutRejectCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheCachedBytes(TEXT("Ias/CacheCachedBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCachePendingBytes(TEXT("Ias/CachePendingBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheReadBytes(TEXT("Ias/CacheReadBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheRejectBytes(TEXT("Ias/CachePutRejectBytes"), TraceCounterDisplayHint_Memory);
// http stats
FCounterInt				GHttpGetCount(TEXT("Ias/HttpGetCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpErrorCount(TEXT("Ias/HttpErrorCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpRetryCount(TEXT("Ias/HttpRetryCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GHttpPendingCount(TEXT("Ias/HttpPendingCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpInflightCount(TEXT("Ias/HttpInflightCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDownloadedBytes(TEXT("Ias/HttpDownloadedBytes"), TraceCounterDisplayHint_Memory);
FCounterInt				GHttpBandwidthMpbs(TEXT("Ias/HttpBandwidthMbps"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDurationMs(TEXT("Ias/HttpDurationMs"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDurationMsAvg(TEXT("Ias/HttpDurationMsAvg"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDurationMsMax(TEXT("Ias/HttpDurationMsMax"), TraceCounterDisplayHint_None);
int64					GHttpDurationMsSum = 0;
constexpr int64			GHttpHistoryCount = 16;
int64					GHttpHistoryDuration[GHttpHistoryCount] = {};
int64					GHttpHistoryBytes[GHttpHistoryCount] = {};
int64					GHttpHistoryTotalDuration = 0;
int64					GHttpHistoryTotalBytes = 0;
int64 					GHttpHistoryIndex = 0;
FIncrementalVariance	GHttpAvgDuration; // Duration of the http requests, in milliseconds
FIncrementalVariance	GHttpAvgRate; // The download rate of the http requests, in MiB/s

////////////////////////////////////////////////////////////////////////////////
// CSV STATS
CSV_DEFINE_CATEGORY(Ias, true);
// iorequest per frame stats
CSV_DEFINE_STAT(Ias, FrameIoRequestCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestReadCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestReadMB);
CSV_DEFINE_STAT(Ias, FrameIoRequestCancelCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestErrorCount);
// cache stat totals
CSV_DEFINE_STAT(Ias, CacheGetCount);
CSV_DEFINE_STAT(Ias, CacheErrorCount);
CSV_DEFINE_STAT(Ias, CachePutCount);
CSV_DEFINE_STAT(Ias, CachePutExistingCount);
CSV_DEFINE_STAT(Ias, CachePutRejectCount);
CSV_DEFINE_STAT(Ias, CacheCachedMB);
CSV_DEFINE_STAT(Ias, CacheReadMB);
CSV_DEFINE_STAT(Ias, CacheRejectedMB);
// http stat totals
CSV_DEFINE_STAT(Ias, HttpGetCount);
CSV_DEFINE_STAT(Ias, HttpRetryCount);
CSV_DEFINE_STAT(Ias, HttpErrorCount);
CSV_DEFINE_STAT(Ias, HttpPendingCount);
CSV_DEFINE_STAT(Ias, HttpDownloadedMB);
CSV_DEFINE_STAT(Ias, HttpBandwidthMpbs);
CSV_DEFINE_STAT(Ias, HttpDurationMsAvg);
CSV_DEFINE_STAT(Ias, HttpDurationMsMax);

static FOnDemandIoBackendStats* GStatistics = nullptr;
static FDelegateHandle GStatisticsEndFrameDelegateHandle;

FOnDemandIoBackendStats::FOnDemandIoBackendStats()
{
	check(GStatistics == nullptr);
	GStatistics = this;
	GStatisticsEndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddLambda([this]()
	{
		// cache stat totals
		CSV_CUSTOM_STAT_DEFINED(CacheGetCount, int32(GCacheGetCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheErrorCount, int32(GCacheErrorCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutCount, int32(GCachePutCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutExistingCount, int32(GCachePutExistingCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutRejectCount, int32(GCachePutRejectCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheCachedMB, BytesToApproxMB(GCacheCachedBytes.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheReadMB, BytesToApproxMB(GCacheReadBytes.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheRejectedMB, BytesToApproxMB(GCacheRejectBytes.Get()), ECsvCustomStatOp::Set);

		// http stat totals
		CSV_CUSTOM_STAT_DEFINED(HttpGetCount, int32(GHttpGetCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpRetryCount, int32(GHttpRetryCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpErrorCount, int32(GHttpErrorCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpPendingCount, int32(GHttpPendingCount.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpDownloadedMB, BytesToApproxMB(GHttpDownloadedBytes.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpBandwidthMpbs, int32(GHttpBandwidthMpbs.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpDurationMsAvg, int32(GHttpDurationMsAvg.Get()), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpDurationMsMax, int32(GHttpDurationMsMax.Get()), ECsvCustomStatOp::Set);
	});
}

FOnDemandIoBackendStats::~FOnDemandIoBackendStats()
{
	FCoreDelegates::OnEndFrame.Remove(GStatisticsEndFrameDelegateHandle);
	GStatistics = nullptr;
}

FOnDemandIoBackendStats* FOnDemandIoBackendStats::Get()
{
	return GStatistics;
}

void FOnDemandIoBackendStats::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	AppendAnalyticsEventAttributeArray(OutAnalyticsArray,
		TEXT("IasHttpErrorCount"), GHttpErrorCount.Get(), 
		TEXT("IasHttpRetryCount"), GHttpRetryCount.Get(),
		TEXT("IasHttpGetCount"), GHttpGetCount.Get(),
		TEXT("IasHttpPendingCount"), GHttpPendingCount.Get(),
		TEXT("IasHttpDownloadedBytes"), GHttpDownloadedBytes.Get(),
		TEXT("IasHttpDurationMeanAvg"), GHttpAvgDuration.GetMean(),
		TEXT("IasHttpDurationStdDev"), GHttpAvgDuration.GetDeviation(),
		TEXT("IasHttpRateMeanAvg"), GHttpAvgRate.GetMean(),
		TEXT("IasHttpRateStdDev"), GHttpAvgRate.GetDeviation(),

		TEXT("IasCacheErrorCount"), GCacheErrorCount.Get(),
		TEXT("IasCacheGetCount"), GCacheGetCount.Get(),
		TEXT("IasCachePutCount"), GCachePutCount.Get(),
		TEXT("IasCachetRejectCount"), GCachePutRejectCount.Get(),
		
		TEXT("IasCacheCachedBytes"), GCacheCachedBytes.Get(),
		TEXT("IasCacheReadBytes"), GCacheReadBytes.Get(),
		TEXT("IasCacheRejectBytes"), GCacheRejectBytes.Get()
	);	
}

void FOnDemandIoBackendStats::OnIoRequestEnqueue()
{
	GIoRequestCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestCount, int32(GIoRequestCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestComplete(uint64 Size, uint64 Duration)
{
	GIoRequestReadCount.Add(1);
	GIoRequestReadBytes.Add(Size);

	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestReadCount, int32(GIoRequestReadCount.Get()), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestReadMB, BytesToApproxMB(GIoRequestReadBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestCancel()
{
	GIoRequestCancelCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestCancelCount, int32(GIoRequestCancelCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestError()
{
	GIoRequestErrorCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestErrorCount, int32(GIoRequestErrorCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCacheError()
{
	GCacheErrorCount.Add(1);
}

void FOnDemandIoBackendStats::OnCacheGet(uint64 DataSize)
{
	GCacheGetCount.Add(1);
	GCacheReadBytes.Add(DataSize);
}

void FOnDemandIoBackendStats::OnCachePut()
{
	GCachePutCount.Add(1);
}

void FOnDemandIoBackendStats::OnCachePutExisting(uint64 /*DataSize*/)
{
	GCachePutExistingCount.Add(1);
}

void FOnDemandIoBackendStats::OnCachePutReject(uint64 DataSize)
{
	GCachePutRejectCount.Add(1);
	GCacheRejectBytes.Add(DataSize);
}

void FOnDemandIoBackendStats::OnCachePendingBytes(uint64 TotalSize)
{
	GCachePendingBytes.Set(TotalSize);
}

void FOnDemandIoBackendStats::OnCachePersistedBytes(uint64 TotalSize)
{
	GCacheCachedBytes.Set(TotalSize);
}

void FOnDemandIoBackendStats::OnHttpEnqueue()
{
	GHttpPendingCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpDequeue()
{
	GHttpInflightCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpGet(uint64 SizeBytes, uint64 DurationMs)
{
	GHttpPendingCount.Add(-1);
	GHttpInflightCount.Add(-1);
	GHttpGetCount.Add(1);
	GHttpDownloadedBytes.Add(SizeBytes);
	GHttpDurationMsSum += DurationMs;
	GHttpDurationMs.Set(DurationMs);

	int64 OldDuration = GHttpHistoryDuration[GHttpHistoryIndex];
	int64 NewDuration = (int64)DurationMs;
	
	GHttpHistoryTotalDuration -= OldDuration;
	GHttpHistoryTotalDuration += NewDuration;
	GHttpHistoryDuration[GHttpHistoryIndex] = NewDuration;

	GHttpHistoryTotalBytes -= GHttpHistoryBytes[GHttpHistoryIndex];
	GHttpHistoryTotalBytes += SizeBytes;
	GHttpHistoryBytes[GHttpHistoryIndex] = SizeBytes;

	GHttpBandwidthMpbs.Set((GHttpHistoryTotalBytes*8)/(GHttpHistoryTotalDuration+1)/1000);
	GHttpDurationMsAvg.Set(GHttpHistoryTotalDuration/GHttpHistoryCount);

	if (GHttpDurationMsMax.Get() < (int64)DurationMs)
	{
		GHttpDurationMsMax.Set((int64)DurationMs);
	}

	GHttpHistoryIndex = (GHttpHistoryIndex + 1) % GHttpHistoryCount;

	GHttpAvgDuration.Increment(static_cast<double>(DurationMs));

	const double SizeMiB = SizeBytes / (1024.0 * 1024.0);
	const double DurationSeconds = DurationMs / 1000.0;

	GHttpAvgRate.Increment(SizeMiB / DurationSeconds);
}

void FOnDemandIoBackendStats::OnHttpRetry()
{
	GHttpPendingCount.Add(-1);
	GHttpInflightCount.Add(-1);
	GHttpRetryCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpError()
{
	GHttpPendingCount.Add(-1);
	GHttpInflightCount.Add(-1);
	GHttpErrorCount.Add(1);
}

} // namespace UE::IO::Private

#endif // IAS_WITH_STATISTICS
