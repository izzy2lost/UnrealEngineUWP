// Copyright Epic Games, Inc. All Rights Reserved.

#include "Statistics.h"
#include "Misc/CoreDelegates.h"

#if IAS_WITH_STATISTICS

namespace UE::IO::Private
{

////////////////////////////////////////////////////////////////////////////////
static int32 BytesToApproxMB(uint64 Bytes) { return int32(Bytes >> 20); }
static int32 BytesToApproxKB(uint64 Bytes) { return int32(Bytes >> 10); }

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
FCounterInt			GIoRequestCount(TEXT("Ias/IoRequestCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GIoRequestReadCount(TEXT("Ias/IoRequestReadCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GIoRequestReadBytes(TEXT("Ias/IoRequestReadBytes"), TraceCounterDisplayHint_Memory);
FCounterInt			GIoRequestCancelCount(TEXT("Ias/IoRequestCancelCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GIoRequestErrorCount(TEXT("Ias/IoRequestErrorCount"), TraceCounterDisplayHint_None);
// cache stats
FCounterAtomicInt	GCacheErrorCount(TEXT("Ias/CacheErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCacheGetCount(TEXT("Ias/CacheGetCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCachePutCount(TEXT("Ias/CachePutCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCachePutExistingCount(TEXT("Ias/CachePutExistingCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCachePutRejectCount(TEXT("Ias/CachePutRejectCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCacheCachedBytes(TEXT("Ias/CacheCachedBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt	GCachePendingBytes(TEXT("Ias/CachePendingBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt	GCacheReadBytes(TEXT("Ias/CacheReadBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt	GCacheRejectBytes(TEXT("Ias/CachePutRejectBytes"), TraceCounterDisplayHint_Memory);
// http stats
FCounterInt			GHttpGetCount(TEXT("Ias/HttpGetCount"), TraceCounterDisplayHint_None);
FCounterInt			GHttpErrorCount(TEXT("Ias/HttpErrorCount"), TraceCounterDisplayHint_None);
FCounterInt			GHttpRetryCount(TEXT("Ias/HttpRetryCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GHttpPendingCount(TEXT("Ias/HttpPendingCount"), TraceCounterDisplayHint_None);
FCounterInt			GHttpInflightCount(TEXT("Ias/HttpInflightCount"), TraceCounterDisplayHint_None);
FCounterInt			GHttpDownloadedBytes(TEXT("Ias/HttpDownloadedBytes"), TraceCounterDisplayHint_Memory);
FCounterInt			GHttpBandwidthMpbs(TEXT("Ias/HttpBandwidthMbps"), TraceCounterDisplayHint_None);
FCounterInt			GHttpDurationMs(TEXT("Ias/HttpDurationMs"), TraceCounterDisplayHint_None);
FCounterInt			GHttpDurationMsAvg(TEXT("Ias/HttpDurationMsAvg"), TraceCounterDisplayHint_None);
FCounterInt			GHttpDurationMsMax(TEXT("Ias/HttpDurationMsMax"), TraceCounterDisplayHint_None);
int64				GHttpDurationMsSum = 0;

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

void FOnDemandIoBackendStats::OnHttpGet(uint64 Size, uint64 DurationMs)
{
	GHttpPendingCount.Add(-1);
	GHttpInflightCount.Add(-1);
	GHttpGetCount.Add(1);
	GHttpDownloadedBytes.Add(Size);
	GHttpDurationMsSum += DurationMs;
	GHttpDurationMs.Set(DurationMs);

	GHttpBandwidthMpbs.Set((GHttpDownloadedBytes.Get()*8)/(GHttpDurationMsSum+1)/1000);
	GHttpDurationMsAvg.Set(GHttpDurationMsSum/GHttpGetCount.Get());
	if (GHttpDurationMsMax.Get() < (int64)DurationMs)
	{
		GHttpDurationMsMax.Set((int64)DurationMs);
	}
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
