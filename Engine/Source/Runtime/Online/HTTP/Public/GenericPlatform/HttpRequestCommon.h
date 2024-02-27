// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/HttpRequestImpl.h"

class IHttpTaskTimerHandle;

/**
 * Contains implementation of some common functions that don't vary between implementations of different platforms
 */
class FHttpRequestCommon : public FHttpRequestImpl
{
public:
	FHttpRequestCommon();

	// IHttpBase
	HTTP_API virtual FString GetURLParameter(const FString& ParameterName) const override;

	// IHttpRequest
	HTTP_API virtual EHttpRequestStatus::Type GetStatus() const override;
	HTTP_API virtual const FString& GetEffectiveURL() const override;
	HTTP_API virtual EHttpFailureReason GetFailureReason() const override;
	HTTP_API virtual void SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy InDelegateThreadPolicy) override;
	HTTP_API virtual EHttpRequestDelegateThreadPolicy GetDelegateThreadPolicy() const override;

	HTTP_API virtual void SetTimeout(float InTimeoutSecs) override;
	HTTP_API virtual void ClearTimeout() override;
	HTTP_API virtual TOptional<float> GetTimeout() const override;
	HTTP_API float GetTimeoutOrDefault() const;

	// Can be called on game thread or http thread depend on the delegate thread policy
	HTTP_API virtual void FinishRequest() = 0;

	HTTP_API virtual void CancelRequest() override;

	HTTP_API virtual void Shutdown() override;

	HTTP_API virtual void ProcessRequestUntilComplete() override;

protected:
	/**
	 * Check if this request is valid or allowed, before actually process the request
	 */
	HTTP_API bool PreProcess();
	HTTP_API void PostProcess();
	HTTP_API virtual bool SetupRequest() = 0;
	HTTP_API bool PreCheck() const;
	HTTP_API virtual void ClearInCaseOfRetry();

	HTTP_API void SetStatus(EHttpRequestStatus::Type InCompletionStatus);
	HTTP_API void SetFailureReason(EHttpFailureReason InFailureReason);

	/**
	 * Finish the request when it's not in http manager
	 */
	HTTP_API void FinishRequestNotInHttpManager();

	HTTP_API void HandleRequestSucceed(TSharedPtr<IHttpResponse> Response);

	HTTP_API void StartActivityTimeoutTimer();
	HTTP_API void StartActivityTimeoutTimerBy(double DelayToTrigger);
	HTTP_API void ResetActivityTimeoutTimer(FStringView Reason);
	HTTP_API void OnActivityTimeoutTimerTaskTrigger();
	HTTP_API void StopActivityTimeoutTimer();
	HTTP_API void StartTotalTimeoutTimer();
	HTTP_API void StopTotalTimeoutTimer();
	HTTP_API void OnTotalTimeoutTimerTaskTrigger();

	HTTP_API virtual void AbortRequest() = 0;

	HTTP_API virtual void CleanupRequest() = 0;

	HTTP_API void TriggerStatusCodeReceivedDelegate(int32 StatusCode);

	HTTP_API void SetEffectiveURL(const FString& InEffectiveURL);

protected:
	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus = EHttpRequestStatus::NotStarted;

	/** Reason of failure of the HTTP request */
	EHttpFailureReason FailureReason = EHttpFailureReason::None;

	/** Thread policy about which thread to complete this request */
	EHttpRequestDelegateThreadPolicy DelegateThreadPolicy = EHttpRequestDelegateThreadPolicy::CompleteOnGameThread;

	/** Timeout in seconds for the entire HTTP request to complete */
	TOptional<float> TimeoutSecs;

	/** Indicate the request is timed out, it should quit and fail with EHttpFailureReason::TimedOut */
	bool bTimedOut = false;
	/** Indicate the request is activity timed out, it should quit and fail with EHttpFailureReason::ConnectionError */
	bool bActivityTimedOut = false;
	/** Indicate the request is cancelled, it should quit and fail with EHttpFailureReason::Cancelled */
	std::atomic<bool> bCanceled = false;

	/** TODO: Move this feature into CurlHttp */
	bool bUsePlatformActivityTimeout = true;

	/** Record when this request started */
	double RequestStartTimeAbsoluteSeconds;

	/** Record when this request will activity timeout */
	double ActivityTimeoutAt;

	/** Holder the timer handle, if the request get destroyed before triggering the timeout, use this to remove the timer */
	TSharedPtr<IHttpTaskTimerHandle> TotalTimeoutHttpTaskTimerHandle;
	/** Holder the timer handle, if the request get destroyed before triggering the timeout, use this to remove the timer */
	TSharedPtr<IHttpTaskTimerHandle> ActivityTimeoutHttpTaskTimerHandle;
	/** Critical section for accessing HttpTaskTimerHandle */
	FCriticalSection HttpTaskTimerHandleCriticalSection;

	/** Record when the request start to process */
	double StartProcessTime = 0.0;

	/** Record how long it take to connect to the endpoint */
	double ConnectTime = -1.0;

	/** Cache the effective URL. When redirected, it will be different with original URL */
	FString EffectiveURL;
};
