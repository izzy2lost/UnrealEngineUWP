// Copyright Epic Games, Inc. All Rights Reserved.

#include "StudioTelemetry.h"
#include "StudioTelemetryLog.h"

#if WITH_EDITOR
#include "StudioTelemetryEditor.h"
#endif

#if !UE_BUILD_SHIPPING
#include "Horde.h"
#endif

#include "Analytics.h"
#include "AnalyticsProviderMulticast.h"
#include "BuildSettings.h"
#include "RHI.h"

#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Thread.h"

#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineBuildSettings.h"

#include "GenericPlatform/GenericPlatformMisc.h"

DEFINE_LOG_CATEGORY(LogStudioTelemetry);

IMPLEMENT_MODULE(FStudioTelemetry, StudioTelemetry)

FStudioTelemetry& FStudioTelemetry::Get()
{
	static FStudioTelemetry StudioTelemetryInstance;
	return StudioTelemetryInstance;
}

void FStudioTelemetry::SetOnEventRecordedCallback(OnEventRecorded InOnEventRecordedCallback )
{
	OnEventRecordedCallback = InOnEventRecordedCallback;

	// If the provider already exists then set the callback
	if (AnalyticsProvider.IsValid())
	{	
		AnalyticsProvider->SetEventCallback(InOnEventRecordedCallback);	
	}
}

void FStudioTelemetry::StartupModule()
{
	UE_LOG(LogStudioTelemetry, Display, TEXT("Starting StudioTelemetry Module"));

	// Create the provider and start the analytics session
	FStudioTelemetry::Get().StartSession();

#if WITH_EDITOR
	// Initialize the analytics subsystems 
	FStudioTelemetryEditor::Get().Initialize();
#endif
}

void FStudioTelemetry::ShutdownModule()
{
#if WITH_EDITOR
	// Shutdown the analytics subsystems
	FStudioTelemetryEditor::Get().Shutdown();
#endif

	// End the session and destroy analytics provider
	FStudioTelemetry::Get().EndSession();

	UE_LOG(LogStudioTelemetry, Display, TEXT("Shutdown StudioTelemetry Module"));
}

void FStudioTelemetry::EndSession()
{
	if (AnalyticsFlowTracker.IsValid())
	{
		AnalyticsFlowTracker->EndSession();
		AnalyticsFlowTracker.Reset();
	}

	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->EndSession();
		AnalyticsProvider.Reset();
	}

	UE_LOG(LogStudioTelemetry, Log, TEXT("Ended StudioTelemetry Session"));
}

void FStudioTelemetry::StartSession()
{
	AnalyticsProvider = FAnalyticsProviderMulticast::CreateAnalyticsProvider();

	if (AnalyticsProvider.IsValid())
	{
		TArray<FAnalyticsEventAttribute> DefaultEventAttributes;

		const FString UserID = FPlatformProcess::UserName(false);
		const FString ProjectName = FApp::GetProjectName();

		FString ProjectIDString;
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"), ProjectIDString, GGameIni);

		FGuid ProjectID(ProjectIDString);

		FString SessionLabel;
		FParse::Value(FCommandLine::Get(), TEXT("SessionLabel="), SessionLabel);

		// Set the default event attributes
		DefaultEventAttributes.Emplace(TEXT("ProjectName"), ProjectName);
		DefaultEventAttributes.Emplace(TEXT("ProjectID"), ProjectID);
		DefaultEventAttributes.Emplace(TEXT("User_ID"), UserID);
		DefaultEventAttributes.Emplace(TEXT("Application_Commandline"), FCommandLine::Get());

		DefaultEventAttributes.Emplace(TEXT("Session_Label"), SessionLabel);
		DefaultEventAttributes.Emplace(TEXT("Session_StartUTC"), FDateTime::UtcNow().ToUnixTimestampDecimal());

		DefaultEventAttributes.Emplace(TEXT("Build_Configuration"), LexToString(FApp::GetBuildConfiguration()));
		DefaultEventAttributes.Emplace(TEXT("Build_IsInternalBuild"), FEngineBuildSettings::IsInternalBuild());
		DefaultEventAttributes.Emplace(TEXT("Build_IsPerforceBuild"), FEngineBuildSettings::IsPerforceBuild());
		DefaultEventAttributes.Emplace(TEXT("Build_IsPromotedBuild"), FApp::GetEngineIsPromotedBuild() == 0 ? false : true);
		DefaultEventAttributes.Emplace(TEXT("Build_BranchName"), FApp::GetBranchName());
		DefaultEventAttributes.Emplace(TEXT("Build_Changelist"), BuildSettings::GetCurrentChangelist());

		DefaultEventAttributes.Emplace(TEXT("Hardware_GPU"), GRHIAdapterName);
		DefaultEventAttributes.Emplace(TEXT("Hardware_CPU"), FPlatformMisc::GetCPUBrand());
		DefaultEventAttributes.Emplace(TEXT("Hardware_CPU_Cores_Physical"), FPlatformMisc::NumberOfCores());
		DefaultEventAttributes.Emplace(TEXT("Hardware_CPU_Cores_Logical"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
		DefaultEventAttributes.Emplace(TEXT("Hardware_RAM"), static_cast<uint64>(FPlatformMemory::GetStats().TotalPhysical));

		DefaultEventAttributes.Emplace(TEXT("Config_IsEditor"), GIsEditor);
		DefaultEventAttributes.Emplace(TEXT("Config_IsUnattended"), FApp::IsUnattended());
		DefaultEventAttributes.Emplace(TEXT("Config_IsBuildMachine"), GIsBuildMachine);
		DefaultEventAttributes.Emplace(TEXT("Config_IsRunningCommandlet"), IsRunningCommandlet());

#if !UE_BUILD_SHIPPING
		DefaultEventAttributes.Emplace(TEXT("Horde_TemplateID"), FHorde::GetTemplateId());
		DefaultEventAttributes.Emplace(TEXT("Horde_TemplateName"), FHorde::GetTemplateName());
		DefaultEventAttributes.Emplace(TEXT("Horde_JobURL"), FHorde::GetJobURL());
		DefaultEventAttributes.Emplace(TEXT("Horde_JobID"), FHorde::GetJobId());
		DefaultEventAttributes.Emplace(TEXT("Horde_StepName"), FHorde::GetStepName());
		DefaultEventAttributes.Emplace(TEXT("Horde_StepID"), FHorde::GetStepId());
		DefaultEventAttributes.Emplace(TEXT("Horde_StepURL"), FHorde::GetStepURL());
		DefaultEventAttributes.Emplace(TEXT("Horde_BatchID"), FHorde::GetBatchId());
#endif

		// Set up the analytics provider
		AnalyticsProvider->SetUserID(UserID);
		AnalyticsProvider->SetDefaultEventAttributes(MoveTemp(DefaultEventAttributes));
		AnalyticsProvider->SetEventCallback(OnEventRecordedCallback);
		
		// Start the analytics session
		AnalyticsProvider->StartSession();

		// Make the flow tracker and start the session
		AnalyticsFlowTracker = MakeShared<FAnalyticsFlowTracker>();
		AnalyticsFlowTracker->SetProvider(AnalyticsProvider);
		AnalyticsFlowTracker->StartSession();

		UE_LOG(LogStudioTelemetry, Log, TEXT("Started StudioTelemetry Session"));
	}
}

void FStudioTelemetry::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (AnalyticsProvider.IsValid())
	{
		FScopeLock ScopeLock(&CriticalSection);
		AnalyticsProvider->RecordEvent(CopyTemp(EventName), Attributes);
	}
}

void FStudioTelemetry::RecordEvent(const FString& ProviderName, const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	TSharedPtr<IAnalyticsProvider> NamedProvider = GetProvider(ProviderName).Pin();

	if (NamedProvider.IsValid())
	{
		NamedProvider->RecordEvent(EventName, Attributes);
	}
}

TWeakPtr<IAnalyticsProvider> FStudioTelemetry::GetProvider()
{
	return AnalyticsProvider;
}

TWeakPtr<IAnalyticsProvider> FStudioTelemetry::GetProvider(const FString& Name)
{
	return AnalyticsProvider.IsValid()? AnalyticsProvider->GetAnalyticsProvider(Name) : TWeakPtr<IAnalyticsProvider>();
}

TWeakPtr<FAnalyticsFlowTracker> FStudioTelemetry::GetFlowTracker()
{
	return AnalyticsFlowTracker;
}