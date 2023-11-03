// Copyright Epic Games, Inc. All Rights Reserved.

#if (IS_PROGRAM || WITH_EDITOR)

#include "PrimeEndpoint.h"

#include "CoreHttp/LatencyTesting.h"
#include "DistributionEndpoints.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/CommandLine.h"
#include "Misc/PathViews.h"
#include "OnDemandHttpClient.h"
#include "OnDemandIoDispatcherBackend.h" // For FOnDemandEndpoint

#include <atomic>

namespace UE::IO::IAS
{

// TODO
extern bool TryParseConfigFile(const FString& ConfigPath, FOnDemandEndpoint& OutEndpoint);

/*** Utility for creating a url to a .iochunk */
static void CreateIoChunkUrl(const FOnDemandTocEntry& Entry, FStringView ChunksDirectory, FAnsiStringBuilderBase& Url)
{
	// Same code as FChunkRequestParams, could factor out to avoid duplication
	const FString HashString = LexToString(Entry.Hash);
	Url << "/" << ChunksDirectory << "/" << HashString.Left(2) << "/" << HashString << ANSITEXTVIEW(".iochunk");
}

/** Find all of the CDN urls available from the distribution URl */
static TIoStatusOr<TArray<FString>> FindCDNUrls(const FString& DistributionUrl)
{
	TArray<FString> CDNUrls;

	FDistributionEndpoints DistributionInfo;
	if (DistributionInfo.ResolveEndpoints(DistributionUrl, CDNUrls) == FDistributionEndpoints::FDistributionEndpoints::EResult::Success)
	{
		return CDNUrls;
	}
	else
	{
		return FIoStatus(EIoErrorCode::Unknown, TEXT("Unable to connect to distributed endpoint"));
	}
}

/** Utility to format time in seconds to a more human readable form */
static FString SecondsToString(uint64 TotalSeconds)
{
	// There is probably code somewhere in the engine for this but I was unable to find
	// any that gave the display we wanted (FText for example was a bit off)
	// Replace with generic code when found.

	const uint64 Hours = (TotalSeconds % 86400) / 3600;
	const uint64 Minutes = (TotalSeconds % 3600) / 60;
	const uint64 Seconds = (TotalSeconds % 60);

	if (Hours > 0)
	{
		return FString::Printf(TEXT("%02dh %02dm %02ds"), Hours, Minutes, Seconds);
	}
	else if (Minutes > 0)
	{
		return FString::Printf(TEXT("%02dm %02ds"), Minutes, Seconds);
	}
	else
	{
		return FString::Printf(TEXT("%02ds"), Seconds);
	}
}

/** Prints the various commands that can be used when priming an endpoint */
static void PrintHelp()
{
	UE_LOG(LogIas, Display, TEXT("PrimeEndPoint Help"));
	UE_LOG(LogIas, Display, TEXT("Required:"));
	UE_LOG(LogIas, Display, TEXT("-PrimeEndPoint=<Path>"));
	UE_LOG(LogIas, Display, TEXT("\tWhere <Path> is an absolute path to the IoStoreOnDemand.ini file you wish to prime"));
	UE_LOG(LogIas, Display, TEXT(""));
	UE_LOG(LogIas, Display, TEXT("Optional:"));
	UE_LOG(LogIas, Display, TEXT("-All"));
	UE_LOG(LogIas, Display, TEXT("\tPrimes all avaliable CDNs rather than just the primary"));
	UE_LOG(LogIas, Display, TEXT("-PrimeUrl=<url>"));
	UE_LOG(LogIas, Display, TEXT("\tPrimes the CDN found at the provided <url>) "));
	UE_LOG(LogIas, Display, TEXT("-List"));
	UE_LOG(LogIas, Display, TEXT("\tPrints a list of avaliable CDNs along with the average latency"));
}

/** Utility to ping a CDN and find the average latency to it */
static int32 PingCDN(const FString& Url, const FString& TocPath)
{
#if !UE_BUILD_SHIPPING
	const uint32 TimeoutMs = 30 * 1000;

	int32 Results[4] = {};
	UE::IO::IAS::HTTP::LatencyTest(Url, TocPath, TimeoutMs, MakeArrayView(Results));

	// Ignore the first result as it might be wildly higher due to caching on the CDN
	return (Results[1] + Results[2] + Results[3]) / 3;
#else
	return -1;
#endif // !UE_BUILD_SHIPPING
}

/** Prints all of the CDNs to screen along with the average latency for each one */
static void ListAvailableCDNs(TArrayView<FString> CDNUrls, const FString& TocPath)
{
	// Collect all ping info up front so we don't delay while printing the list
	TArray<int32> Pings;
	Pings.SetNum(CDNUrls.Num());

	for (int32 Index = 0; Index < CDNUrls.Num(); ++Index)
	{
		Pings[Index] = PingCDN(CDNUrls[Index], TocPath);
	}

	UE_LOG(LogIas, Display, TEXT("Listing CDN Urls:"));
	for (int32 Index = 0; Index < CDNUrls.Num(); ++Index)
	{
		UE_LOG(LogIas, Display, TEXT("%02d: %s (Ping %d ms)"), Index + 1, *CDNUrls[Index], Pings[Index]);
	}
}

/** Primes a single CDN */
static FIoStatus PrimeCDN(FString CDNUrl, const FPrimeEndpointArgs& Args)
{
	const double StartTime = FPlatformTime::Seconds();

	CDNUrl = CDNUrl.Replace(TEXT("https"), TEXT("http"));

	UE_LOG(LogIas, Display, TEXT("Priming '%s'"), *CDNUrl);
	
	UE_LOG(LogIas, Display, TEXT("\tDownloading Toc from CDN..."));
	TIoStatusOr<FOnDemandToc> TocResult = LoadTocFromUrl(CDNUrl, Args.TocPath, 3);
	if (!TocResult.IsOk())
	{
		return TocResult.Status();
	}

	FOnDemandToc Toc = TocResult.ConsumeValueOrDie();

	TArray<FOnDemandTocEntry> Entries;
	for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
	{
		Entries.Append(Container.Entries);
	}

	// Need to build the directory where the chunks are stored based on the Toc path and info from it's header
	TStringBuilder<512> ChunksDirectory;
	{
		int32 Idx = INDEX_NONE;
		if (Args.TocPath.FindLastChar(TCHAR('/'), Idx))
		{
			ChunksDirectory << FStringView(Args.TocPath).Left(Idx);
		}

		FPathViews::Append(ChunksDirectory, Toc.Header.ChunksDirectory);
	}

	UE_LOG(LogIas, Display, TEXT("\tToc contained %d entries to prime"), Entries.Num());

	// Note the max number of connections that FEventLoop supports is  (2^6)-1
	const int32 NumConnections = 63;

	UE_LOG(LogIas, Display, TEXT("\tPriming entries with %d connections..."), NumConnections);

	{
		auto AnsiUrl = StringCast<ANSICHAR>(*CDNUrl);

		using namespace UE::IO::IAS::HTTP;

		FConnectionPool::FParams PoolParams;
		PoolParams.SetHostFromUrl(AnsiUrl);
		PoolParams.ConnectionCount = NumConnections;
		FConnectionPool Pool(PoolParams);

		FEventLoop Loop;

		std::atomic<int32> Inflight = 0;
		std::atomic<int32> Completed = 0;
		std::atomic<int64> TotalQueryTimeMs = 0;

		double TotalTime = FPlatformTime::Seconds();
		double Timer = FPlatformTime::Seconds();

		// Called when we are waiting for requests to complete
		auto BusyLoop = [&Loop, &Completed, &Timer,&TotalTime, &TotalQueryTimeMs, Total = Entries.Num()]()
			{
				Loop.Tick(-1);

				const double DisplayFrequency = 10.0;
				if (FPlatformTime::Seconds() - Timer >= DisplayFrequency)
				{
					const double ItemsPerSecond = Completed / (FPlatformTime::Seconds() - TotalTime);
					const double TimeEstimate = static_cast<double>(Total - Completed) / ItemsPerSecond;
					const FString EstimateString = SecondsToString(static_cast<uint64>(TimeEstimate));

					const int64 AvgQueryTimeMs = TotalQueryTimeMs / Completed;

					UE_LOG(LogIas, Display, TEXT("\tCompleted %d/%d | Avg Query (ms) %lld | Est Time Remaining: %s"), Completed.load(), Total, AvgQueryTimeMs , *EstimateString);
					Timer = FPlatformTime::Seconds();
				}
			};

		for (const FOnDemandTocEntry& Entry : Entries)
		{
			TAnsiStringBuilder<256> Url;
			CreateIoChunkUrl(Entry, ChunksDirectory, Url);

			while (Inflight >= NumConnections)
			{
				BusyLoop();
			}

			FRequest Request = Loop.Request("HEAD", Url, Pool);
			Loop.Send(MoveTemp(Request), [&Completed, &Inflight, &TotalQueryTimeMs, StartTime = FPlatformTime::Seconds()](const FTicketStatus& Status)
				{
					const FTicketStatus::EId StatusId = Status.GetId();
					if (StatusId == FTicketStatus::EId::Response)
					{
						return;
					}

					const double QueryTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
					TotalQueryTimeMs += FMath::RoundToInt(QueryTimeMs);

					Completed++;
					--Inflight;
				});

			Inflight++;
		}

		// Wait for the remaining inflight requests to finish up
		while (Completed != Entries.Num())
		{
			BusyLoop();
		}
	}

	const double TotalTimeTaken = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogIas, Display, TEXT("\tPriming took %s"), *SecondsToString(static_cast<uint64>(TotalTimeTaken)));

	return FIoStatus::Ok;
}

/** Primes all of the given CDNs */
FIoStatus PrimeAllCDNs(TArray<FString> Urls, const FPrimeEndpointArgs& Args)
{
	if (Urls.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::NotFound, TEXT("No CDN urls were found!"));
	}

	for (const FString& Url : Urls)
	{
		PrimeCDN(Url, Args);
	}

	return FIoStatus::Ok;
}

FIoStatus PrimeEndpointInternal(const FString& IoStoreOnDemandIniPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PrimeEndpointInternalArgsIni);

	if (FParse::Param(FCommandLine::Get(), TEXT("Help")))
	{
		PrintHelp();
		return FIoStatus::Ok;
	}

	FOnDemandEndpoint Endpoint;

	UE_LOG(LogIas, Display, TEXT("Parsing '%s'..."), *IoStoreOnDemandIniPath);
	if (!TryParseConfigFile(IoStoreOnDemandIniPath, Endpoint))
	{
		return FIoStatus(EIoErrorCode::Unknown, TEXT("Failed to parse config file"));
	}

	FPrimeEndpointArgs Args(MoveTemp(Endpoint.DistributionUrl), MoveTemp(Endpoint.TocPath));
	return PrimeEndpointInternal(Args);
}

FIoStatus PrimeEndpointInternal(const FPrimeEndpointArgs & Args)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PrimeEndpointInternalArgs);

	TIoStatusOr<TArray<FString>> Result = FindCDNUrls(Args.DistributionUrl);
	if (!Result.IsOk())
	{
		return Result.Status();
	}

	TArray<FString> CDNUrls = Result.ConsumeValueOrDie();

	FString CmdlineUrl;
	if (FParse::Param(FCommandLine::Get(), TEXT("PrimeAll")))
	{
		return PrimeAllCDNs(CDNUrls, Args);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("PrimeUrl="), CmdlineUrl))
	{
		return PrimeCDN(CmdlineUrl, Args);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("List")))
	{
		ListAvailableCDNs(CDNUrls, Args.TocPath);
		return FIoStatus::Ok;
	}
	else
	{
		if (CDNUrls.IsEmpty())
		{
			return FIoStatus(EIoErrorCode::NotFound, TEXT("No CDN urls were found!"));
		}

		return PrimeCDN(CDNUrls[0], Args);
	}
}

} // namespace UE::IO::IAS

#endif // (IS_PROGRAM || WITH_EDITOR)
