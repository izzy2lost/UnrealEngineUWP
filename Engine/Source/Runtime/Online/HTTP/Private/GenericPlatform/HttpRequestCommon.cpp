// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpRequestCommon.h"
#include "GenericPlatform/HttpResponseCommon.h"
#include "Http.h"
#include "HttpManager.h"

FString FHttpRequestCommon::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(GetURL(), ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

EHttpRequestStatus::Type FHttpRequestCommon::GetStatus() const
{
	return CompletionStatus;
}

EHttpFailureReason FHttpRequestCommon::GetFailureReason() const
{
	return FailureReason;
}

bool FHttpRequestCommon::PreCheck() const
{
	// Prevent overlapped requests using the same instance
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
		return false;
	}

	// Nothing to do without a valid URL
	if (GetURL().IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
		return false;
	}

	if (GetVerb().IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No Verb was specified."));
		return false;
	}

	if (!FHttpModule::Get().GetHttpManager().IsDomainAllowed(GetURL()))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not using an allowed domain."), *GetURL());
		return false;
	}

	return true;
}

void FHttpRequestCommon::SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy InDelegateThreadPolicy)
{ 
	DelegateThreadPolicy = InDelegateThreadPolicy; 
}

EHttpRequestDelegateThreadPolicy FHttpRequestCommon::GetDelegateThreadPolicy() const
{ 
	return DelegateThreadPolicy; 
}

void FHttpRequestCommon::SetStatus(EHttpRequestStatus::Type InCompletionStatus)
{
	CompletionStatus = InCompletionStatus;

	if (FHttpResponsePtr Response = GetResponse())
	{
		TSharedPtr<FHttpResponseCommon> ResponseCommon = StaticCastSharedPtr<FHttpResponseCommon>(Response);
		ResponseCommon->SetRequestStatus(InCompletionStatus);
	}
}

void FHttpRequestCommon::SetFailureReason(EHttpFailureReason InFailureReason)
{
	FailureReason = InFailureReason;

	if (FHttpResponsePtr Response = GetResponse())
	{
		TSharedPtr<FHttpResponseCommon> ResponseCommon = StaticCastSharedPtr<FHttpResponseCommon>(Response);
		ResponseCommon->SetRequestFailureReason(InFailureReason);
	}
}