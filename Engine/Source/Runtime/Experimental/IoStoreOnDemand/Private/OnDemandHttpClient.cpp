// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandHttpClient.h"

#include "Containers/StringConv.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IO/IoBuffer.h"
#include "IO/IoStoreOnDemand.h"

namespace UE::IO::IAS
{

int32 GIasHttpRateLimitKiBPerSecond = 0;
static FAutoConsoleVariableRef CVar_GIasHttpRateLimitKiBPerSecond(
	TEXT("ias.HttpRateLimitKiBPerSecond"),
	GIasHttpRateLimitKiBPerSecond,
	TEXT("Http throttle limit in KiBPerSecond")
);

int32 GIasHttpPollTimeoutMs = 17;
static FAutoConsoleVariableRef CVar_GIasHttpPollTimeoutMs(
	TEXT("ias.HttpPollTimeoutMs"),
	GIasHttpPollTimeoutMs,
	TEXT("Http tick poll timeout in milliseconds")
);

static void LogHttpResult(const TCHAR* Url, uint32 StatusCode, uint64 DurationMs, uint64 Size, uint64 Offset, const char* Memo = "ok")
{
	Size >>= 10;
	UE_LOG(LogIas, VeryVerbose, TEXT("http-%3u: %5" UINT64_FMT "ms %5" UINT64_FMT "KiB[%7" UINT64_FMT "] '%S' %s"), StatusCode, DurationMs, Size, Offset, Memo, Url);
};

FOnDemandHttpClient::FOnDemandHttpClient(const FString& ServiceUrl, int32 MaxConnectionCount)
	: SvcsUrl(ServiceUrl)
	, MaxConnections(MaxConnectionCount)
{
	auto ServiceUrlAnsi = StringCast<ANSICHAR>(*ServiceUrl, ServiceUrl.Len());

	HTTP::FConnectionPool::FParams Params;
	if (Params.SetHostFromUrl(ServiceUrlAnsi) < 0)
	{
		UE_LOG(LogIas, Error, TEXT("Failed to set host from '%s'"), *ServiceUrl);
	}

	Params.ConnectionCount = MaxConnectionCount;
	ConnectionPool = MakeUnique<HTTP::FConnectionPool>(Params);
}

void FOnDemandHttpClient::Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FGetCallback&& Callback)
{
	Issue(Url, MoveTemp(Callback), Range);
}

void FOnDemandHttpClient::Get(FAnsiStringView Url, FGetCallback&& Callback)
{
	Issue(Url, MoveTemp(Callback));
}

void FOnDemandHttpClient::Issue(FAnsiStringView Url, FGetCallback&& Callback, FIoOffsetAndLength Range)
{
	using namespace UE::IO::IAS::HTTP;

	auto Sink = [
		Buffer = FIoBuffer(),
			Callback = MoveTemp(Callback),
			Url = FString(Url),
			Offset = Range.GetOffset(),
			StartTime = FPlatformTime::Cycles64(),
			StatusCode = uint32(0)]
			(const FTicketStatus& Status) mutable
	{
		if (FTicketStatus::EId::Response == Status.GetId())
		{
			FResponse& Response = Status.GetResponse();
			StatusCode = Response.GetStatusCode();
			Response.SetDestination(&Buffer);
		}
		else if (FTicketStatus::EId::Content == Status.GetId())
		{
			const uint64 DurationMs = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
			const FIoBuffer& Content = Status.GetContent();

			LogHttpResult(*Url, StatusCode, DurationMs, Content.GetSize(), Offset);

			const bool bSuccessful = StatusCode > 199 && StatusCode < 300;
			if (bSuccessful && Content.GetSize() > 0)
			{
				Callback(Content, DurationMs);
			}
			else
			{
				Callback(FIoStatus(EIoErrorCode::NotFound, TEXTVIEW("Invalid Content")), DurationMs);
			}
		}
		else if (FTicketStatus::EId::Error == Status.GetId())
		{
			const uint64 DurationMs = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
			LogHttpResult(*Url, StatusCode, DurationMs, 0, Offset, Status.GetErrorReason());
			Callback(FIoStatus(EIoErrorCode::ReadError, FString(Status.GetErrorReason())), DurationMs);
		}
	};

		UE::IO::IAS::HTTP::FRequest Request = EventLoop.Get(Url, *ConnectionPool);
		const uint64 RangeStart = Range.GetOffset();
		const uint64 RangeEnd = Range.GetOffset() + Range.GetLength();
		if (RangeStart > 0 || RangeEnd > 0)
		{
			Request.Header(ANSITEXTVIEW("Range"), WriteToAnsiString<64>(ANSITEXTVIEW("bytes="), RangeStart, ANSITEXTVIEW("-"), RangeEnd));
		}

		EventLoop.Send(MoveTemp(Request), MoveTemp(Sink));
}

bool FOnDemandHttpClient::Tick(bool Block)
{
	int32 TimeoutMs = Block ? -1 : GIasHttpPollTimeoutMs;
	EventLoop.Throttle(GIasHttpRateLimitKiBPerSecond);
	return EventLoop.Tick(TimeoutMs) != 0;
}

} //namespace UE::IO::IAS
