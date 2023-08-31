// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistributionEndpoints.h"

#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "IO/IoStoreOnDemand.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeRWLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Statistics.h"

namespace UE::IO::IAS
{

extern int32 GIasMaxHttpRetryCount;

FDistributionEndpoints::~FDistributionEndpoints()
{
	CancelEndpointRequests();
}

#if IS_PROGRAM || WITH_EDITOR

bool FDistributionEndpoints::Flush(double TimeOut)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistributionEndpoints::Flush);

	if (!bInitialized)
	{
		ResolveDeferredEndpoints();
	}

	FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();

	const double StartTime = FPlatformTime::Seconds();

	while (!PendingRequests.IsEmpty())
	{
		HttpManager.Tick(0.0);

		FPlatformProcess::SleepNoStats(0.0f);

		if (TimeOut > 0.0 && (FPlatformTime::Seconds() - StartTime) > TimeOut)
		{
			return false;
		}
	}

	return true;
}

#endif // IS_PROGRAM || WITH_EDITOR

void FDistributionEndpoints::ResolveEndpoints(const FString& DistributionUrl, FOnEndpointResolved&& OnResolved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::ResolveEndpoints);
	const FResolvedEndpoint* Ep = nullptr;
	{
		FReadScopeLock _(Lock);
		if (TUniquePtr<FResolvedEndpoint>* Entry = ResolvedEndpoints.Find(DistributionUrl))
		{
			Ep = Entry->Get();
		}
	}

	if (Ep != nullptr)
	{
		return OnResolved(DistributionUrl, Ep->ServiceUrls);
	}

	bool bIssueRequest = false;
	{
		FWriteScopeLock _(Lock);
		TUniquePtr<FResolveRequest>& Request = PendingRequests.FindOrAdd(DistributionUrl);
		if (!Request.IsValid())
		{
			Request.Reset(new FResolveRequest{ DistributionUrl });
			bIssueRequest = bInitialized;
		}
		Request->Callbacks.Add(MoveTemp(OnResolved));
	}

	if (bIssueRequest)
	{
		IssueEndpointRequests();
	}
}

void FDistributionEndpoints::ResolveDeferredEndpoints()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::ResolveDeferredEndpoints);
	{
		FWriteScopeLock _(Lock);
		bInitialized = true;
	}

	IssueEndpointRequests();
}

void FDistributionEndpoints::IssueEndpointRequests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::IssueEndpointRequests);
	// Currently we need to use the HTTP module in order to resolve service endpoints due to HTTPS
	FHttpModule& HttpModule = FHttpModule::Get();
	const int32 MaxAttempts = GIasMaxHttpRetryCount;

	TArray<FHttpRequestPtr, TInlineAllocator<2>> HttpRequests;
	{
		FWriteScopeLock _(Lock);
		check(bInitialized);

		for (const TPair<FString, TUniquePtr<FResolveRequest>>& Kv : PendingRequests)
		{
			if (Kv.Value->HttpRequest.IsValid())
			{
				continue;
			}

			FResolveRequest& ResolveRequest = *Kv.Value.Get();
			UE_LOG(LogIas, Log, TEXT("Resolving '%s' (#%d/%d)"), *ResolveRequest.DistributionUrl, ResolveRequest.RetryCount + 1, MaxAttempts);

			FHttpRequestPtr HttpRequest = HttpModule.CreateRequest();
			HttpRequest->SetTimeout(3.0f);
			HttpRequest->SetURL(Kv.Key);
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
			HttpRequest->OnProcessRequestComplete().BindLambda(
				[this, &ResolveRequest, MaxAttempts](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
				{
					LLM_SCOPE_BYTAG(Ias);
					FHttpRequestPtr Request = MoveTemp(ResolveRequest.HttpRequest);

					// Response will be null if the connection timed out
					if (Response == nullptr || Response->GetResponseCode() != 200)
					{
						if (++ResolveRequest.RetryCount <= MaxAttempts)
						{
							Request->OnProcessRequestComplete().Unbind();
							return IssueEndpointRequests();
						}
					}

					CompleteEndpointRequest(ResolveRequest, Response);
				});

			ResolveRequest.HttpRequest = HttpRequest;
			HttpRequests.Add(HttpRequest);
		}
	}

	for (FHttpRequestPtr& Request : HttpRequests)
	{
		Request->ProcessRequest();
	}
}

void FDistributionEndpoints::CancelEndpointRequests()
{
	TArray<FHttpRequestPtr, TInlineAllocator<2>> HttpRequests;
	{
		FWriteScopeLock _(Lock);
		for (const TPair<FString, TUniquePtr<FResolveRequest>>& Kv : PendingRequests)
		{
			if (Kv.Value->HttpRequest.IsValid())
			{
				HttpRequests.Add(Kv.Value->HttpRequest);
			}
		}
	}

	if (!HttpRequests.IsEmpty())
	{
		FHttpModule& HttpModule = FHttpModule::Get();
		for (FHttpRequestPtr& Request : HttpRequests)
		{
			HttpModule.GetHttpManager().RemoveRequest(Request.ToSharedRef());
			//TODO: Flush?
		}
	}
}

void FDistributionEndpoints::CompleteEndpointRequest(FResolveRequest& ResolveRequest, FHttpResponsePtr HttpResponse)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::CompleteEndpointRequest);

	using FJsonValuePtr = TSharedPtr<FJsonValue>;
	using FJsonObjPtr = TSharedPtr<FJsonObject>;
	using FJsonReader = TJsonReader<TCHAR>;
	using FJsonReaderPtr = TSharedRef<FJsonReader>;

	TArray<FString> ServiceUrls;
	if (HttpResponse->GetResponseCode() == 200)
	{
		FString Json = HttpResponse->GetContentAsString();
		FJsonReaderPtr JsonReader = TJsonReaderFactory<TCHAR>::Create(Json);

		FJsonObjPtr JsonObj;
		if (FJsonSerializer::Deserialize(JsonReader, JsonObj))
		{
			TArray<FJsonValuePtr> JsonValues = JsonObj->GetArrayField(TEXT("distributions"));
			for (const FJsonValuePtr& JsonValue : JsonValues)
			{
				FString ServiceUrl = JsonValue->AsString();
				if (ServiceUrl.EndsWith(TEXT("/")))
				{
					ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
				}
				ServiceUrls.Add(MoveTemp(ServiceUrl));
			}
		}
	}

	const FResolvedEndpoint* ResolvedEndpoint = nullptr;
	FString DistributionUrl;
	TArray<FOnEndpointResolved> Callbacks;

	{
		FWriteScopeLock _(Lock);
		if (!ServiceUrls.IsEmpty())
		{
			ResolvedEndpoint = ResolvedEndpoints.Emplace(
				ResolveRequest.DistributionUrl,
				new FResolvedEndpoint{ MoveTemp(ServiceUrls) })
				.Get();
		}

		Callbacks = MoveTemp(ResolveRequest.Callbacks);
		DistributionUrl = MoveTemp(ResolveRequest.DistributionUrl);
		PendingRequests.Remove(DistributionUrl);
	}

	TConstArrayView<FString> Urls = ResolvedEndpoint ? ResolvedEndpoint->ServiceUrls : TConstArrayView<FString>();
	for (FOnEndpointResolved& Callback : Callbacks)
	{
		Callback(DistributionUrl, Urls);
	}
}

} // namespace UE::IO::IAS
