// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalyticsProviderConfigurationDelegate.h"
#include "IAnalyticsProviderET.h"

/**
 * Implementation of the IAnalyticsProviderET interface that forwards the API calls to an array of IAnalyticsProviderET interfaces
 */
class FAnalyticsProviderMulticast : public IAnalyticsProviderET
{
public:

	using TProviders = TMap<FString, TSharedPtr<IAnalyticsProviderET>>;

	FAnalyticsProviderMulticast();

	static TSharedPtr<FAnalyticsProviderMulticast> CreateAnalyticsProvider();

	virtual void SetShouldRecordEventFunc(const ShouldRecordEventFunction& ShouldRecordEventFunc) override;
	virtual bool StartSession(FString InSessionID, const TArray<FAnalyticsEventAttribute>& Attributes = {}) override;
	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes = {}) override;
	virtual void EndSession() override;

	virtual bool ShouldRecordEvent(const FString& EventName) const override;
	virtual void FlushEvents() override;
	virtual void BlockUntilFlushed(float InTimeoutSec) override;

	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)  override;
	virtual TArray<FAnalyticsEventAttribute> GetDefaultEventAttributesSafe() const override;
	virtual int32 GetDefaultEventAttributeCount() const  override;
	virtual FAnalyticsEventAttribute GetDefaultEventAttribute(int AttributeIndex) const  override;

	virtual void SetURLEndpoint(const FString& UrlEndpoint, const TArray<FString>& AltDomains)  override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void SetAppID(FString&& AppID) override;
	virtual void SetAppVersion(FString&& AppVersion) override;
	virtual void SetEventCallback(const OnEventRecorded& Callback)  override;
	virtual bool SetSessionID(const FString& InSessionID) override;
	virtual void SetUserID(const FString& InUserID) override;

	virtual FString GetSessionID() const override;
	virtual FString GetUserID() const override;
	virtual const FAnalyticsET::Config& GetConfig() const  override;

	virtual void RecordEvent(FString&& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	
	bool HasValidProviders() const { return Providers.Num() > 0; }
	TWeakPtr<IAnalyticsProviderET> GetAnalyticsProvider(const FString& Name);

private:

	TProviders								Providers;
	FAnalyticsET::Config					Config;
	FString									UserID;
	FString									SessionID;
	TArray<FAnalyticsEventAttribute>		DefaultEventAttributes;
	ShouldRecordEventFunction				ShouldRecordEventFunc;
	OnEventRecorded							OnEventRecordedCallback;
};
