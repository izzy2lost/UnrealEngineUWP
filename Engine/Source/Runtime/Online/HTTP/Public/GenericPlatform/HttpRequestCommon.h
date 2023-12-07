// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/HttpRequestImpl.h"

/**
 * Contains implementation of some common functions that don't vary between implementations of different platforms
 */
class FHttpRequestCommon : public FHttpRequestImpl
{
public:
	// IHttpBase
	HTTP_API virtual FString GetURLParameter(const FString& ParameterName) const override;

	// IHttpRequest
	HTTP_API virtual EHttpRequestStatus::Type GetStatus() const override;
	HTTP_API virtual EHttpFailureReason GetFailureReason() const override;
	HTTP_API virtual void SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy InDelegateThreadPolicy) override;
	HTTP_API virtual EHttpRequestDelegateThreadPolicy GetDelegateThreadPolicy() const override;

	HTTP_API virtual void SetTimeout(float InTimeoutSecs) override;
	HTTP_API virtual void ClearTimeout() override;
	HTTP_API virtual TOptional<float> GetTimeout() const override;
	HTTP_API float GetTimeoutOrDefault() const;

	// Can be called on game thread or http thread depend on the delegate thread policy
	HTTP_API virtual void FinishRequest() = 0;

protected:
	/**
	 * Check if this request is valid or allowed, before actually process the request
	 */
	HTTP_API bool PreProcess();
	HTTP_API virtual bool SetupRequest() = 0;
	HTTP_API bool PreCheck() const;
	HTTP_API void ClearInCaseOfRetry();

	HTTP_API void SetStatus(EHttpRequestStatus::Type InCompletionStatus);
	HTTP_API void SetFailureReason(EHttpFailureReason InFailureReason);

	/**
	 * Finish the request when it's not in http manager
	 */
	HTTP_API void FinishRequestNotInHttpManager();

protected:
	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus = EHttpRequestStatus::NotStarted;

	/** Reason of failure of the HTTP request */
	EHttpFailureReason FailureReason = EHttpFailureReason::None;

	/** Thread policy about which thread to complete this request */
	EHttpRequestDelegateThreadPolicy DelegateThreadPolicy = EHttpRequestDelegateThreadPolicy::CompleteOnGameThread;

	/** Timeout in seconds for the entire HTTP request to complete */
	TOptional<float> TimeoutSecs;

	/** Record when this request started */
	double RequestStartTimeAbsoluteSeconds;
};
