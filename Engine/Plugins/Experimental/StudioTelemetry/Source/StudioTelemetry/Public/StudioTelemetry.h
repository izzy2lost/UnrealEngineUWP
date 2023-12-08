// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IAnalyticsProvider.h"
#include "AnalyticsFlowTracker.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FAnalyticsProviderMulticast;

/**
 * Public facing Studio Telemetry Plugin API
 */
class FStudioTelemetry : public IModuleInterface
{
public:

	typedef TFunction<void(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attrs)> OnRecordEvent;

	/** Check whether the module is available*/
	static STUDIOTELEMETRY_API bool IsAvaliable() { return FModuleManager::Get().IsModuleLoaded("StudioTelemetry"); }

	/** Access to the module singleton*/
	static STUDIOTELEMETRY_API FStudioTelemetry& Get();

	/** Access to the a specific named analytics provider within the system*/
	STUDIOTELEMETRY_API TWeakPtr<IAnalyticsProvider> GetProvider(const FString& ProviderName);

	/** Access to the broadcast analytics provider for the system*/
	STUDIOTELEMETRY_API TWeakPtr<IAnalyticsProvider> GetProvider();

	/** Access to the flow tracker for the system, this will ultimately broadcast flow events to all providers*/
	STUDIOTELEMETRY_API TWeakPtr<FAnalyticsFlowTracker> GetFlowTracker();
	
	/** Thread safe method to record an event to all analytics providers*/
	STUDIOTELEMETRY_API void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes = {});

	/** Thread safe method to record an event to the named analytics provider */
	STUDIOTELEMETRY_API void RecordEvent(const FString& ProviderName, const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes = {});
	
	/** Method for custom recording of telemetry events*/
	STUDIOTELEMETRY_API void SetRecordEventCallback(OnRecordEvent);

private:

	/** IModuleInterface implementation */
	STUDIOTELEMETRY_API virtual void StartupModule()  final;
	STUDIOTELEMETRY_API virtual void ShutdownModule()  final;

	/** Starts a new analytics session*/
	void StartSession();

	/** Ends an existing analytics session*/
	void EndSession();

	FCriticalSection						CriticalSection;
	TSharedPtr<FAnalyticsProviderMulticast>	AnalyticsProvider;
	TSharedPtr<FAnalyticsFlowTracker>		AnalyticsFlowTracker;
	OnRecordEvent							RecordEventCallback;
};
