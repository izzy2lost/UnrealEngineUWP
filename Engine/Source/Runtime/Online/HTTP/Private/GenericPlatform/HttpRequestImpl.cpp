// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpRequestImpl.h"
#include "Stats/Stats.h"
#include "Http.h"

FHttpRequestCompleteDelegate& FHttpRequestImpl::OnProcessRequestComplete()
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FHttpRequestProgressDelegate& FHttpRequestImpl::OnRequestProgress() 
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnRequestProgress()"));
	return RequestProgressDelegate;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FHttpRequestProgressDelegate64& FHttpRequestImpl::OnRequestProgress64() 
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnRequestProgress64()"));
	return RequestProgressDelegate64;
}

FHttpRequestHeaderReceivedDelegate& FHttpRequestImpl::OnHeaderReceived()
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnHeaderReceived()"));
	return HeaderReceivedDelegate;
}

FHttpRequestWillRetryDelegate& FHttpRequestImpl::OnRequestWillRetry()
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnRequestWillRetry()"));
	return OnRequestWillRetryDelegate;
}

void FHttpRequestImpl::SetTimeout(float InTimeoutSecs)
{
	TimeoutSecs = InTimeoutSecs;
}

void FHttpRequestImpl::ClearTimeout()
{
	TimeoutSecs.Reset();
}

TOptional<float> FHttpRequestImpl::GetTimeout() const
{
	return TimeoutSecs;
}

float FHttpRequestImpl::GetTimeoutOrDefault() const
{
	return GetTimeout().Get(FHttpModule::Get().GetHttpTimeout());
}

void FHttpRequestImpl::BroadcastResponseHeadersReceived()
{
	if (OnHeaderReceived().IsBound())
	{
		const FHttpResponsePtr Response = GetResponse();
		if (Response.IsValid())
		{
			const FHttpRequestPtr ThisPtr(SharedThis(this));
			const TArray<FString> AllHeaders = Response->GetAllHeaders();
			for (const FString& Header : AllHeaders)
			{
				FString HeaderName;
				FString HeaderValue;
				if (Header.Split(TEXT(":"), &HeaderName, &HeaderValue))
				{
					HeaderValue.TrimStartInline();

					QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRequestImpl_BroadcastResponseHeadersReceived_OnHeaderReceived);
					OnHeaderReceived().ExecuteIfBound(ThisPtr, HeaderName, HeaderValue);
				}
			}
		}
	}
}
