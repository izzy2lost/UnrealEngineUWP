// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandHttpClient.h"

#include "Containers/StringConv.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IO/IoBuffer.h"
#include "IO/IoStoreOnDemand.h"

namespace UE::IO::IAS
{

static void LogHttpResult(const TCHAR* Host, const TCHAR* Url, uint32 StatusCode, uint64 DurationMs, uint64 Size, uint64 Offset, const char* Memo = "ok")
{
	Size >>= 10;
	UE_LOG(LogIas, VeryVerbose, TEXT("http-%3u: %5" UINT64_FMT "ms %5" UINT64_FMT "KiB[%7" UINT64_FMT "] '%S' %s%s"), StatusCode, DurationMs, Size, Offset, Memo, Host, Url);
};

void FHttpClient::Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FGetCallback&& Callback)
{
	check(PrimaryConnection != INDEX_NONE);
	IssueRequest(FRequestParams
	{
		.Url = FString(Url),
		.Range = Range,
		.Callback = MoveTemp(Callback),
		.Connection = PrimaryConnection
	});
}

void FHttpClient::Get(FAnsiStringView Url, FGetCallback&& Callback)
{
	Get(Url, FIoOffsetAndLength(), MoveTemp(Callback));
}

bool FHttpClient::Tick(uint32 WaitTimeMs, uint32 MaxKiBPerSecond)
{
	if (PrimaryConnection == INDEX_NONE)
	{
		return false;
	}

	EventLoop.Throttle(MaxKiBPerSecond);
	const uint32 TicketCount = EventLoop.Tick(WaitTimeMs);

	if (Retries.IsEmpty() == false)
	{
		const int32 RequestCount = FMath::Min(Retries.Num(), int32(HTTP::FEventLoop::MaxActiveTickets - TicketCount));
		for (int32 Idx = 0; Idx < RequestCount; Idx++)
		{
			IssueRequest(MoveTemp(Retries[Idx]));
		}
		Retries.RemoveAtSwap(0, RequestCount);
	}

	const bool bIsIdle = EventLoop.IsIdle();
	if (bIsIdle)
	{
		for (int32 Idx = 0, Count = Connections.Num(); Idx < Count; ++Idx)
		{
			if (Idx != PrimaryConnection)
			{
				Connections[Idx].Reset();
			}
		}
	}

	return bIsIdle == false;
}

FHttpClient::FHttpClient(FHttpClientConfig&& ClientConfig)
	: Config(MoveTemp(ClientConfig))
{
	Configure();
}

TUniquePtr<FHttpClient> FHttpClient::Create(FHttpClientConfig&& ClientConfig)
{
	if (ClientConfig.Endpoints.IsEmpty())
	{
		return TUniquePtr<FHttpClient>();
	}

	for (const FString& Ep : ClientConfig.Endpoints)
	{
		if (Ep.IsEmpty())
		{
			return TUniquePtr<FHttpClient>();
		}

		//TODO: Get rid of all string conversions
		if (UE::IO::IAS::HTTP::FConnectionPool::IsValidHostUrl(StringCast<ANSICHAR>(*Ep, Ep.Len())) == false)
		{
			return TUniquePtr<FHttpClient>();
		}
	}

	return TUniquePtr<FHttpClient>(new FHttpClient(MoveTemp(ClientConfig)));
}

TUniquePtr<FHttpClient> FHttpClient::Create(const FString& Endpoint)
{
	FHttpClientConfig Config;
	Config.Endpoints.Add(Endpoint);
	return Create(MoveTemp(Config));
}

void FHttpClient::Configure()
{
	check(Config.Endpoints.IsEmpty() == false);
	Connections.SetNum(Config.Endpoints.Num());
	Connections[Config.PrimaryEndpoint] = CreateConnection(Config.Endpoints[Config.PrimaryEndpoint]);
	PrimaryConnection = Config.PrimaryEndpoint;
}

void FHttpClient::IssueRequest(FRequestParams&& Params)
{
	using namespace UE::IO::IAS::HTTP;

	//TODO: Remove string conversion
	const auto Url = StringCast<ANSICHAR>(*Params.Url, Params.Url.Len());

	auto Sink = [
		this,
		Params = MoveTemp(Params),
		Buffer = FIoBuffer(),
		StartTime = FPlatformTime::Cycles64(),
		StatusCode = uint32(0)]
		(const FTicketStatus& Status) mutable
		{
			const uint64 DurationMs = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
			switch (Status.GetId())
			{
			case FTicketStatus::EId::Response:
			{
				FResponse& Response = Status.GetResponse();
				StatusCode = Response.GetStatusCode();
				Response.SetDestination(&Buffer);
				break;
			}
			case FTicketStatus::EId::Content:
			{
				const FIoBuffer& Content = Status.GetContent();
				LogHttpResult(*Config.Endpoints[Params.Connection], *Params.Url, StatusCode, DurationMs, Content.GetSize(), Params.Range.GetOffset());
				
				const bool bSuccessful = StatusCode > 199 && StatusCode < 300;
				if (bSuccessful && Content.GetSize() > 0)
				{
					Params.Callback(Content, DurationMs);
					if (Params.Attempt > 1 && Params.Connection != PrimaryConnection)
					{
						UE_LOG(LogIas, Log, TEXT("HTTP client endpoint changed from '%s' to '%s'"),
							*Config.Endpoints[PrimaryConnection], *Config.Endpoints[Params.Connection]);
						PrimaryConnection = Params.Connection;
					}
				}
				else
				{
					const bool bServerError = StatusCode > 499 && StatusCode < 600;
					if (Params.Attempt < Config.MaxRetryCount)
					{
						const bool bNextConnection = bServerError == false && Params.Attempt > 0;
						RetryRequest(MoveTemp(Params), bNextConnection);
					}
					else
					{
						const EIoErrorCode ErrorCode = bServerError ? EIoErrorCode::ReadError : EIoErrorCode::NotFound;
						Params.Callback(FIoStatus(ErrorCode), DurationMs);
					}
				}
				break;
			}
			case FTicketStatus::EId::Error:
			{
				if (Params.Attempt < Config.MaxRetryCount)
				{
					const bool bNextConnection = Params.Attempt > 0;
					RetryRequest(MoveTemp(Params), bNextConnection);
				}
				else
				{
					LogHttpResult(*Config.Endpoints[Params.Connection], *Params.Url, StatusCode, DurationMs, 0, Params.Range.GetOffset(), Status.GetErrorReason());
					Params.Callback(FIoStatus(EIoErrorCode::ReadError, FString(Status.GetErrorReason())), DurationMs);
				}
				break;
			}
			default:
			{
				LogHttpResult(*Config.Endpoints[Params.Connection], *Params.Url, StatusCode, DurationMs, 0, Params.Range.GetOffset(), "cancelled");
				Params.Callback(FIoStatus(EIoErrorCode::Cancelled, FString(Status.GetErrorReason())), DurationMs);
				break;
			}
			}
		};

	TUniquePtr<FConnectionPool>& Connection = Connections[Params.Connection];
	check(Connection.IsValid());

	FRequest Request = EventLoop.Get(Url, *Connection);

	if (Params.Range.GetOffset() > 0 || Params.Range.GetLength() > 0)
	{
		Request.Header(ANSITEXTVIEW("Range"),
			WriteToAnsiString<64>(ANSITEXTVIEW("bytes="), Params.Range.GetOffset(), ANSITEXTVIEW("-"), Params.Range.GetOffset() + Params.Range.GetLength()));
	}

	EventLoop.Send(MoveTemp(Request), MoveTemp(Sink));
}

void FHttpClient::RetryRequest(FRequestParams&& Params, bool bNextConnection)
{
	check(Params.Attempt < Config.MaxRetryCount);
	if (bNextConnection && Config.Endpoints.Num() > 1)
	{
		Params.Connection = (Params.Connection + 1) % Config.Endpoints.Num();
		if (Connections[Params.Connection].IsValid() == false)
		{
			Connections[Params.Connection] = CreateConnection(Config.Endpoints[Params.Connection]);
		}
	}
	Params.Attempt++;
	Retries.Emplace(MoveTemp(Params));
}

TUniquePtr<HTTP::FConnectionPool> FHttpClient::CreateConnection(const FStringView& HostAddr)
{
	HTTP::FConnectionPool::FParams Params;
	ensure(Params.SetHostFromUrl(StringCast<ANSICHAR>(HostAddr.GetData(), HostAddr.Len())) >= 0);
	if (Config.ReceiveBufferSize >= 0)
	{
		UE_LOG(LogIas, Log, TEXT("HTTP client receive buffer size set to %d"), Config.ReceiveBufferSize);
		Params.RecvBufSize = Config.ReceiveBufferSize;
	}
	Params.ConnectionCount = Config.MaxConnectionCount;

	return MakeUnique<HTTP::FConnectionPool>(Params);
}

} //namespace UE::IO::IAS
