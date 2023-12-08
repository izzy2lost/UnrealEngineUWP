// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "StudioTelemetry.h"

class FTelemetryRouter;

/**
 * A class that implements a variety of pre-configured Core and Editor telemetry events that can be used to evaluate the efficiecnty of the most common developer workflows
 */
class FStudioTelemetryEditor : FNoncopyable
{
public:
	FStudioTelemetryEditor() {};
	~FStudioTelemetryEditor() {};

	static FStudioTelemetryEditor& Get();
	void Initialize();
	void Shutdown();

private:	

	/** Various event handling functions */
	static void RecordEvent_Cooking(TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_Loading(const FString& LoadingName, double LoadingSeconds, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_CoreSystems(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_DDCResource(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_DDCSummary(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_IAS(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_Zen(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RecordEvent_VirtualAssets(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes = {});
	static void RegisterCollectionWorkflowDelegates(FTelemetryRouter& Router);

	/** Internal variables for Flows and Timer data */
	FString LevelName;
	FGuid EditorBootFlowGuid;
	FGuid InteractiveEditorFlowGuid;
	FGuid LoadMapSubFlowGuid;
	FGuid PIEFlowGuid;
	FGuid PIEInitializeSubFlowGuid;
	FGuid PIELoadMapSubFlowGuid;
	FGuid CookByTheBookFlowGuid;

	double SessionStartTime;
	double PIEStartTime;
	double LoadMapStartTime;
	double AssetOpenStartTime;
	double WorldStreamingStartTime;
	double EditorStartupTime=0;
	double LoadMapTime=0;
	double PIEStartupTime = 0;
	double PIELoadMapTime = 0;
	double PIELoadMapStartTime;
};

#endif // WITH_EDITOR

