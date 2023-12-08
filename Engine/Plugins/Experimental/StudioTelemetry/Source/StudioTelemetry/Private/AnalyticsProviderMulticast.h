// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalyticsProviderConfigurationDelegate.h"
#include "Interfaces/IAnalyticsProvider.h"

/**
 * Implementation of the IAnalyticsProvider interface that forwards the API calls to an array of IAnalyticsProviderET interfaces
 */
class FAnalyticsProviderMulticast : public IAnalyticsProvider
{
public:

	using TProviders = TMap<FString, TSharedPtr<IAnalyticsProvider>>;

	typedef TFunction<void(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attrs)> OnRecordEvent;


	FAnalyticsProviderMulticast();

	static TSharedPtr<FAnalyticsProviderMulticast> CreateAnalyticsProvider();

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes = {}) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;
	
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)  override;
	virtual TArray<FAnalyticsEventAttribute> GetDefaultEventAttributesSafe() const override;
	virtual int32 GetDefaultEventAttributeCount() const  override;
	virtual FAnalyticsEventAttribute GetDefaultEventAttribute(int AttributeIndex) const  override;

	virtual bool SetSessionID(const FString& InSessionID) override;
	virtual void SetUserID(const FString& InUserID) override;

	virtual FString GetSessionID() const override;
	virtual FString GetUserID() const override;
	
	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;	
	void SetRecordEventCallback(OnRecordEvent Callback);
	bool HasValidProviders() const { return Providers.Num() > 0; }
	TWeakPtr<IAnalyticsProvider> GetAnalyticsProvider(const FString& Name);

private:

	TProviders								Providers;
	FString									UserID;
	FString									SessionID;
	TArray<FAnalyticsEventAttribute>		DefaultEventAttributes;
	OnRecordEvent							RecordEventCallback;
};
