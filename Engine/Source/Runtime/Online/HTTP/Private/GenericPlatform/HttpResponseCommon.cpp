// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpResponseCommon.h"
#include "GenericPlatform/HttpRequestCommon.h"
#include "GenericPlatform/GenericPlatformHttp.h"

FHttpResponseCommon::FHttpResponseCommon(const FHttpRequestCommon& HttpRequest)
	: URL(HttpRequest.GetURL())
	, CompletionStatus(HttpRequest.GetStatus())
{
}

FString FHttpResponseCommon::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(URL, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

FString FHttpResponseCommon::GetURL() const
{
	return URL;
}

void FHttpResponseCommon::SetRequestStatus(EHttpRequestStatus::Type InCompletionStatus)
{
	CompletionStatus = InCompletionStatus;
}

EHttpRequestStatus::Type FHttpResponseCommon::GetStatus() const
{
	return CompletionStatus;
}
