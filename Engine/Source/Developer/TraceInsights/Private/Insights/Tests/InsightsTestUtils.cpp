// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/Tests/InsightsTestUtils.h"

#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManagerGeneric.h"

#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/ModuleService.h"
#include "Insights/Common/Stopwatch.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/InsightsManager.h"

#include "Misc/AutomationTest.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

FInsightsTestUtils::FInsightsTestUtils(FAutomationTestBase* InTest) :
	Test(InTest)
{
#if WITH_EDITOR
	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	TSharedPtr<TraceServices::IModuleService> ModuleService = TraceServicesModule.GetModuleService();
	ModuleService->SetModuleEnabled(FName("TraceModule_LoadTimeProfiler"), true);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::AnalyzeTrace(const TCHAR* Path) const
{
	IUnrealInsightsModule& TraceInsightsModule = FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");

	if (!FPaths::FileExists(Path))
	{
		Test->AddError(FString::Printf(TEXT("File does not exist: %s."), Path));
		return false;
	}

	TraceInsightsModule.StartAnalysisForTraceFile(Path);
	auto Session = TraceInsightsModule.GetAnalysisSession();
	if (Session == nullptr)
	{
		Test->AddError(TEXT("Session analysis failed to start."));
		return false;
	}

	FStopwatch StopWatch;
	StopWatch.Start();

	double Duration = 0.0f;
	constexpr double MaxDuration = 75.0f;
	while (!Session->IsAnalysisComplete())
	{
		FPlatformProcess::Sleep(0.033f);

		if (Duration > MaxDuration)
		{
			Test->AddError(FString::Format(TEXT("Session analysis took longer than the maximum allowed time of {0} seconds. Aborting test."), { MaxDuration }));
			return false;
		}

		StopWatch.Update();
		Duration = StopWatch.GetAccumulatedTime();
	}

	StopWatch.Stop();
	Duration = StopWatch.GetAccumulatedTime();

	Test->AddInfo(FString::Format(TEXT("Session analysis took {0} seconds."), { Duration }));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::FileContainsString(const FString& PathToFile, const FString& ExpectedString, const float& Timeout) const
{
	float StartTime = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - StartTime) < Timeout)
	{
		if (!FPaths::FileExists(PathToFile))
		{
			FPlatformProcess::Sleep(0.1f);
		}
		else
		{
			FString LogFileContents;
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*PathToFile, true)); // Open the file with shared read access
			if (FileHandle)
			{
				TArray<uint8> FileData;
				FileData.SetNumUninitialized(FileHandle->Size());
				FileHandle->Read(FileData.GetData(), FileData.Num());
				FFileHelper::BufferToString(LogFileContents, FileData.GetData(), FileData.Num());

				if (LogFileContents.Contains(ExpectedString))
				{
					return true;
				}
			}
			FPlatformProcess::Sleep(0.1f);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::StartTracing(FTraceAuxiliary::EConnectionType ConnectionType, const float& Timeout) const
{
	bool bStarted = false;
	if (ConnectionType == FTraceAuxiliary::EConnectionType::Network)
	{
		bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, TEXT("localhost"), nullptr);
	}
	else if (ConnectionType == FTraceAuxiliary::EConnectionType::File)
	{
		bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, nullptr, nullptr);
	}

	double TraceVerifyStartTime = FPlatformTime::Seconds();
	while (bStarted && (FPlatformTime::Seconds() - TraceVerifyStartTime < Timeout))
	{
		FPlatformProcess::Sleep(0.5f);
		if (FTraceAuxiliary::IsConnected())
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::SetupUTS(const float& Timeout) const
{
	const FString UnrealTraceServerName = TEXT("UnrealTraceServer");

	FString UTSPath = FPlatformProcess::GenerateApplicationPath("UnrealTraceServer", EBuildConfiguration::Development);
	FString UTSParameters = TEXT("daemon");
	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;
	uint32 ProcessID = 0;
	const int32 PriorityModifier = 0;
	const TCHAR* OptionalWorkingDirectory = nullptr;
	void* PipeWriteChild = nullptr;
	void* PipeReadChild = nullptr;
	FProcHandle UTSHandle = FPlatformProcess::CreateProc(*UTSPath, *UTSParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
	if (!UTSHandle.IsValid())
	{
		return false;
	}

	double StartTime = FPlatformTime::Seconds();
	while (FPlatformTime::Seconds() - StartTime < Timeout)
	{
		if (FPlatformProcess::IsApplicationRunning(*UnrealTraceServerName))
		{
			return true;
		}
		UTSHandle = FPlatformProcess::CreateProc(*UTSPath, *UTSParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
		FPlatformProcess::Sleep(0.1f);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::KillUTS(const float& Timeout) const
{
	const FString UnrealTraceServerName = TEXT("UnrealTraceServer");

	FString UTSPath = FPlatformProcess::GenerateApplicationPath("UnrealTraceServer", EBuildConfiguration::Development);
	FString UTSParameters = TEXT("kill");
	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;
	uint32 ProcessID = 0;
	const int32 PriorityModifier = 0;
	const TCHAR* OptionalWorkingDirectory = nullptr;
	void* PipeWriteChild = nullptr;
	void* PipeReadChild = nullptr;
	FProcHandle UTSHandle = FPlatformProcess::CreateProc(*UTSPath, *UTSParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
	if (!UTSHandle.IsValid())
	{
		return false;
	}

	double StartTime = FPlatformTime::Seconds();
	while (FPlatformTime::Seconds() - StartTime < Timeout)
	{
		if (!FPlatformProcess::IsApplicationRunning(*UnrealTraceServerName))
		{
			return true;
		}
		UTSHandle = FPlatformProcess::CreateProc(*UTSPath, *UTSParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild); 
		FPlatformProcess::Sleep(0.1f);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsTestUtils::ResetSession() const
{
	TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
	InsightsManager->ResetSession();
}