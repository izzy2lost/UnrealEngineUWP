// Copyright Epic Games, Inc. All Rights Reserved.

#include "UITests.h"

#include "HAL/PlatformFileManager.h"

#include "Insights/Common/Stopwatch.h"

#include "TraceServices/Model/TimingProfiler.h"
#include "Insights/InsightsManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocFilterValueConverter.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/TimeFilterValueConverter.h"
#include "Insights/Widgets/STimingProfilerWindow.h"
#include "Insights/Widgets/SStartPageWindow.h"
#include "Insights/MemoryProfiler/Widgets/SMemoryProfilerWindow.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/Widgets/STimingView.h"
#include "Insights/Tests/InsightsTestUtils.h"
#include "Insights/TaskGraphProfiler/ViewModels/TaskTimingTrack.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#if !WITH_EDITOR

////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_LOG_CATEGORY(UITests);

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FHideAndShowAllTimingViewTabs::RunTest(const FString& Parameters)
{
	TSharedPtr<FTimingProfilerManager> TimingProfilerManager = FTimingProfilerManager::Get();

	TimingProfilerManager->ShowHideTimingView(false);
	TimingProfilerManager->ShowHideCalleesTreeView(false);
	TimingProfilerManager->ShowHideCallersTreeView(false);
	TimingProfilerManager->ShowHideFramesTrack(false);
	TimingProfilerManager->ShowHideLogView(false);
	TimingProfilerManager->ShowHideTimersView(false);

	TimingProfilerManager->ShowHideTimingView(true);
	TimingProfilerManager->ShowHideCalleesTreeView(true);
	TimingProfilerManager->ShowHideCallersTreeView(true);
	TimingProfilerManager->ShowHideFramesTrack(true);
	TimingProfilerManager->ShowHideLogView(true);
	TimingProfilerManager->ShowHideTimersView(true);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAutomationDriverUnrealInsightsSessionBrowserTest::Define()
{
	BeforeEach([this]() {
		AutomationWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		const FString AutomationWindowName = TEXT("Automation");
		if (AutomationWindow->GetTitle().ToString().Contains(AutomationWindowName))
		{
			AutomationWindow->Minimize();
		}

		if (IAutomationDriverModule::Get().IsEnabled())
		{
			IAutomationDriverModule::Get().Disable();
		}
		IAutomationDriverModule::Get().Enable();

		Driver = IAutomationDriverModule::Get().CreateDriver();		
		});

	// TestRail: C28843555
	Describe("CopyRenameDeleteTrace", [this]()
		{
			It("should verify that user copy, rename and delete traces", EAsyncExecution::ThreadPool, [this]()
				{
					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
					TestTrue("Insigts manager should not be null", InsightsManager.IsValid());
					InsightsManager->GetTraceStoreWindow()->SetDeleteTraceConfirmationWindowVisibility(false);

					const FString StoreDir = InsightsManager->GetStoreDir();
					const FString ProjectDir = FPaths::ProjectDir();

					const FString SourceTestTracePath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/Test.utrace");
					const FString SourceTestCachePath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/Test.ucache");

					FString StoreTestTracePath = StoreDir / TEXT("Test.utrace");
					FString StoreTestCachePath = StoreDir / TEXT("Test.ucache");

					TestTrue("Trace in project exists", PlatformFile.FileExists(*SourceTestTracePath));
					TestTrue("Cache in project exists", PlatformFile.FileExists(*SourceTestCachePath));

					TestFalse("Trace in store should not exist before copy", PlatformFile.FileExists(*StoreTestTracePath));
					TestFalse("Cache in store should not exist before copy", PlatformFile.FileExists(*StoreTestCachePath));

					// Copy trace
					// Here we just check that button can be clicked. Unable to copy and paste via Automation Driver 
					FDriverElementRef ExploreTraceStoreDirButton = Driver->FindElement(By::Id("ExploreTraceStoreDirButton"));
					TestTrue("Explore Trace Store Dir Button clicked", ExploreTraceStoreDirButton->IsInteractable());

					PlatformFile.CopyFile(*StoreTestTracePath, *SourceTestTracePath);
					PlatformFile.CopyFile(*StoreTestCachePath, *SourceTestCachePath);

					TestTrue("Trace copied", PlatformFile.FileExists(*StoreTestTracePath));
					TestTrue("Cache copied", PlatformFile.FileExists(*StoreTestCachePath));

					StoreTestTracePath = StoreDir / TEXT("TestUcacheRenaming.utrace");
					StoreTestCachePath = StoreDir / TEXT("TestUcacheRenaming.ucache");

					TestFalse("Renamed trace should not exist before renaming", PlatformFile.FileExists(*StoreTestTracePath));
					TestFalse("Renamed cache should not exist before renaming", PlatformFile.FileExists(*StoreTestCachePath));

					// Rename 
					int Index = 0;
					auto TraceWaiter = [Driver = Driver, &Index](void) -> bool
					{
						auto Elements = Driver->FindElements(By::Id("TraceList"))->GetElements();
						for (int i = 0; i < Elements.Num(); ++i) {
							if (Elements[i]->GetText().ToString() == TEXT("Test")) {
								Index = i;
								return true;
							}
						}
						return false;
					};

					bool bTestTraceExists = Driver->Wait(Until::Condition(TraceWaiter, FWaitTimeout::InSeconds(10)));
					if (!bTestTraceExists)
					{
						AddError("Trace should exists in Session Browser");
						return;
					}

					FDriverElementRef TraceElement = Driver->FindElements(By::Id("TraceList"))->GetElements()[Index];

					FDriverSequenceRef Sequence = Driver->CreateSequence();
					Sequence->Actions()
						.Click(TraceElement)
						.Type(EKeys::F2)
						.Type(TEXT("UcacheRenaming"))
						.Type(EKeys::Enter);

					TestTrue("Trace renamed", Sequence->Perform());

					TestTrue("Renamed trace should exists", PlatformFile.FileExists(*StoreTestTracePath));
					TestTrue("Renamed cache should exists", PlatformFile.FileExists(*StoreTestCachePath));

					// Delete
					FDriverElementRef OpenTraceButton = Driver->FindElement(By::Id("OpenTraceButton"));
					Driver->Wait(Until::ElementIsInteractable(OpenTraceButton, FWaitTimeout::InSeconds(10)));

					TraceElement = Driver->FindElements(By::Id("TraceList"))->GetElements()[0];
					TraceElement->Type(EKeys::Delete);

					TestFalse("Renamed trace should be deleted", PlatformFile.FileExists(*StoreTestTracePath));
					TestFalse("Renamed cache should be deleted", PlatformFile.FileExists(*StoreTestCachePath));
				});

			AfterEach([this]() {
				TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
				const FString StoreDir = InsightsManager->GetStoreDir();
				const FString StoreTestTracePath = StoreDir / TEXT("Test.utrace");
				const FString StoreTestCachePath = StoreDir / TEXT("Test.ucache");
				IFileManager::Get().Delete(*StoreTestTracePath, false, true);
				IFileManager::Get().Delete(*StoreTestCachePath, false, true);
				});
		});

	// TestRail: C35547680
	Describe("MemoryInsights.XMLReportsUpload", [this]()
		{
			It("should verify that user can upload xml reports in Memory Insights tab", EAsyncExecution::ThreadPool, FTimespan::FromSeconds(120), [this]()
				{
					FInsightsTestUtils Utils(this);
					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
					if (!InsightsManager.IsValid())
					{
						AddError("Insigts manager should not be null");
						return;
					}

					// Start tracing editor instance, not Lyra. There is no difference between them in this test.
					FString UEPath = FPlatformProcess::GenerateApplicationPath("UnrealEditor", EBuildConfiguration::Development);
					FString Parameters = TEXT("-trace=Bookmark,Memory -tracehost=127.0.0.1");
					constexpr bool bLaunchDetached = true;
					constexpr bool bLaunchHidden = false;
					constexpr bool bLaunchReallyHidden = false;
					uint32 ProcessID = 0;
					const int32 PriorityModifier = 0;
					const TCHAR* OptionalWorkingDirectory = nullptr;
					void* PipeWriteChild = nullptr;
					void* PipeReadChild = nullptr;
					FProcHandle EditorHandle = FPlatformProcess::CreateProc(*UEPath, *Parameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
					if (!EditorHandle.IsValid())
					{
						AddError("Editor should be started");
						return;
					}

					// Verify that LIVE trace appeared
					int Index = 0;
					auto TraceWaiter = [Driver = Driver, &Index](void) -> bool
					{
						auto Elements = Driver->FindElements(By::Id("TraceStatusColumnList"))->GetElements();
						for (int i = 0; i < Elements.Num(); ++i) {
							if (Elements[i]->GetText().ToString() == TEXT("LIVE")) {
								Index = i;
								return true;
							}
						}
						return false;
					};

					if (!Driver->Wait(Until::Condition(TraceWaiter, FWaitTimeout::InSeconds(10))))
					{
						AddError("Live trace should appear");
						FPlatformProcess::TerminateProc(EditorHandle);
						return;
					}

					FDriverElementRef TraceElement = Driver->FindElements(By::Id("TraceList"))->GetElements()[Index];
					const FString TraceName = TraceElement->GetText().ToString();

					const FString StoreDir = InsightsManager->GetStoreDir();
					const FString ProjectDir = FPaths::ProjectDir();
					const FString StoreTracePath = StoreDir / FString::Printf(TEXT("%s.utrace"), *TraceName);
					const FString StoreCachePath = StoreDir / FString::Printf(TEXT("%s.ucache"), *TraceName);
					const FString LogDirPath = ProjectDir / TEXT("TestResults");
					const FString LogPath = ProjectDir / TEXT("TestResults/Log.txt");
					const FString SuccessTestResult = TEXT("Test Completed. Result={Success}");

					// Test live trace
					FString TraceParameters = FString::Printf(TEXT("-InsightsTest -ABSLOG=\"%s\" -AutoQuit -ExecOnAnalysisCompleteCmd=\"Automation RunTests Insights.UploadMemoryInsightsLLMXMLReportsTrace\" -OpenTraceFile=%s"), *LogPath, *StoreTracePath);
					InsightsManager->OpenUnrealInsights(*TraceParameters);
					bool bLineFound = Utils.FileContainsString(LogPath, SuccessTestResult, 60.0f);
					TestTrue("Test for live trace should pass", bLineFound);

					check(IFileManager::Get().DeleteDirectory(*LogDirPath, false, true));
					FPlatformProcess::TerminateProc(EditorHandle);

					// Test stopped trace 
					InsightsManager->OpenUnrealInsights(*TraceParameters);
					bLineFound = Utils.FileContainsString(LogPath, SuccessTestResult, 60.0f);
					TestTrue("Test for stopped trace should pass", bLineFound);

					check(IFileManager::Get().DeleteDirectory(*LogDirPath, false, true));
					check(IFileManager::Get().Delete(*StoreTracePath, false, true));
					check(IFileManager::Get().Delete(*StoreCachePath, false, true));
				});
		});
	AfterEach([this]() {
		Driver.Reset();
		IAutomationDriverModule::Get().Disable();
		AutomationWindow->Restore();
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// TestRail: C35547680
bool FMemoryInsightsUploadLLMXMLReportsTraceTest::RunTest(const FString& Parameters)
{
	const FString ReportGraphsXMLPath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/ReportGraphs.xml");
	const FString LLMReportTypesXMLPath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/LLMReportTypes.xml");

	FMemorySharedState* SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	if (!SharedState)
	{
		AddError("ProfilerWindow should be valid. Please, run this test throught Insights Session automation tab");
		return false;
	}

	SharedState->RemoveAllMemTagGraphTracks();
	const int DefaultTracksAmount = SharedState->GetTimingView()->GetAllTracks().Num();
	SharedState->RemoveAllMemTagGraphTracks();
	AddExpectedError("Failed to load Report");
	SharedState->CreateTracksFromReport(ReportGraphsXMLPath);
	SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	const int AfterReportGraphsUploadTrackAmount = SharedState->GetTimingView()->GetAllTracks().Num();
	TestTrue("Tracks amount should be default ", DefaultTracksAmount == AfterReportGraphsUploadTrackAmount);

	SharedState->RemoveAllMemTagGraphTracks();
	SharedState->CreateTracksFromReport(LLMReportTypesXMLPath);
	SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	const int AfterLLMReportTypesUploadTrackAmount = SharedState->GetTimingView()->GetAllTracks().Num();
	TestTrue("Tracks should not be default", DefaultTracksAmount != AfterLLMReportTypesUploadTrackAmount);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// TestRail: T223693543
bool FUnrealTraceServerStartingTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
	const FString UnrealTraceServerName = TEXT("UnrealTraceServer");
	const float Timeout = 10.0f;

	auto WaitForUTSProcess = [](const FString& ProcessName, const float Timeout, const bool ExpectedStatus) -> bool
	{
		double StartTime = FPlatformTime::Seconds();
		while (FPlatformTime::Seconds() - StartTime < Timeout)
		{
			if (FPlatformProcess::IsApplicationRunning(*ProcessName) == ExpectedStatus)
			{
				return ExpectedStatus;
				break;
			}
			FPlatformProcess::Sleep(0.1f);
		}
		return !ExpectedStatus;
	};

	auto WaitForConnectionIconStatus = [](TSharedPtr<class FInsightsManager> InsightsManager, const float Timeout, const bool ExpectedStatus) -> bool
	{
		double StartTime = FPlatformTime::Seconds();
		while (FPlatformTime::Seconds() - StartTime < Timeout)
		{
			TSharedPtr<STraceStoreWindow> Wnd = InsightsManager->GetTraceStoreWindow();
			if (Wnd.IsValid())
			{
				if (Wnd->IsConnected() == ExpectedStatus)
				{
					return ExpectedStatus;
					break;
				}
			}
			FPlatformProcess::Sleep(0.1f);
		}
		return !ExpectedStatus;
	};

	bool bIsUnrealTraceServerRunning = WaitForUTSProcess(UnrealTraceServerName, Timeout, true);
	TestTrue("UnrealTraceServer should be in processes", bIsUnrealTraceServerRunning);

	bool bConnectionStatus = WaitForConnectionIconStatus(InsightsManager, Timeout, true);
	TestTrue("Connection status should be true", bConnectionStatus);

#if PLATFORM_WINDOWS
	FString Command = FString::Printf(TEXT("/F /IM \"%s.exe\""), *UnrealTraceServerName);
	FPlatformProcess::ExecProcess(TEXT("taskkill"), *Command, nullptr, nullptr, nullptr);
#elif PLATFORM_MAC || PLATFORM_LINUX
	FString Command = FString::Printf(TEXT("killall \"%s\""), *UnrealTraceServerName);
	FPlatformProcess::ExecProcess(TEXT("/bin/sh"), *Command, nullptr, nullptr, nullptr);
#endif

	bIsUnrealTraceServerRunning = WaitForUTSProcess(UnrealTraceServerName, Timeout, false);
	TestFalse("UnrealTraceServer should not be in processes", bIsUnrealTraceServerRunning);

	bConnectionStatus = WaitForConnectionIconStatus(InsightsManager, Timeout, false);
	TestFalse("Connection status should be false", bConnectionStatus);

	FString UEPath = FPlatformProcess::GenerateApplicationPath("UnrealEditor", EBuildConfiguration::Development);
	FString UEParameters = TEXT("");
	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;
	uint32 ProcessID = 0;
	const int32 PriorityModifier = 0;
	const TCHAR* OptionalWorkingDirectory = nullptr;
	void* PipeWriteChild = nullptr;
	void* PipeReadChild = nullptr;
	FProcHandle EditorHandle = FPlatformProcess::CreateProc(*UEPath, *UEParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
	if (!EditorHandle.IsValid())
	{
		AddError("Editor should be started");
		return true;
	}

	bIsUnrealTraceServerRunning = WaitForUTSProcess(UnrealTraceServerName, Timeout, true);
	TestTrue("UnrealTraceServer should be in processes", bIsUnrealTraceServerRunning);

	bConnectionStatus = WaitForConnectionIconStatus(InsightsManager, Timeout, true);
	TestTrue("Connection status should be true", bConnectionStatus);

	FPlatformProcess::TerminateProc(EditorHandle);

#if PLATFORM_WINDOWS
	FPlatformProcess::ExecProcess(TEXT("taskkill"), *Command, nullptr, nullptr, nullptr);
#elif PLATFORM_MAC || PLATFORM_LINUX
	FPlatformProcess::ExecProcess(TEXT("/bin/sh"), *Command, nullptr, nullptr, nullptr);
#endif

	bIsUnrealTraceServerRunning = WaitForUTSProcess(UnrealTraceServerName, Timeout, false);
	TestFalse("UnrealTraceServer should not be in processes", bIsUnrealTraceServerRunning);

	FString Path = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/r423_win64_game_10478456.utrace");
	InsightsManager->OpenUnrealInsights(TEXT("-InsightsTest -AutoQuit -NoUI -OpenTraceFile=%s"));
	bIsUnrealTraceServerRunning = WaitForUTSProcess(UnrealTraceServerName, Timeout, true);
	TestTrue("UnrealTraceServer should be in processes", bIsUnrealTraceServerRunning);

	return true;
}

#endif // !WITH_EDITOR

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FMemoryFilterValueConverterTest::RunTest(const FString& Parameters)
{
	Insights::FMemoryFilterValueConverter Converter;

	FText Error;
	int64 Value;

	Converter.Convert(TEXT("152485"), Value, Error);
	TestEqual(TEXT("BasicValue"), 152485LL, Value);

	Converter.Convert(TEXT("125.56"), Value, Error);
	TestEqual(TEXT("DoubleValue"), 125LL, Value);

	Converter.Convert(TEXT("3 KiB"), Value, Error);
	TestEqual(TEXT("Kib"), 3072LL, Value);

	Converter.Convert(TEXT("7.14 KiB"), Value, Error);
	TestEqual(TEXT("KibDouble"), 7311LL, Value);

	Converter.Convert(TEXT("5 MiB"), Value, Error);
	TestEqual(TEXT("Mib"), 5242880LL, Value);

	Converter.Convert(TEXT("1 EiB"), Value, Error);
	TestEqual(TEXT("Eib"), 1152921504606846976LL, Value);

	Converter.Convert(TEXT("2 kib"), Value, Error);
	TestEqual(TEXT("CaseInsesitive"), 2048LL, Value);

	TestFalse(TEXT("Fail1"), Converter.Convert(TEXT("23test"), Value, Error));
	TestFalse(TEXT("Fail2"), Converter.Convert(TEXT("43 kOb"), Value, Error));
	TestFalse(TEXT("FailInvalidChar"), Converter.Convert(TEXT("45,"), Value, Error));

	return !HasAnyErrors();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTimeFilterValueConverterTest::RunTest(const FString& Parameters)
{
	Insights::FTimeFilterValueConverter Converter;

	FText Error;
	double Value;

	Converter.Convert(TEXT("15.3"), Value, Error);
	TestEqual(TEXT("ValueInSeconds"), 15.3, Value);

	Converter.Convert(TEXT("0.2"), Value, Error);
	TestEqual(TEXT("ValueInSeconds2"), 0.2, Value);

	Converter.Convert(TEXT("0.4s"), Value, Error);
	TestEqual(TEXT("ValueInSeconds3"), 0.4, Value);

	Converter.Convert(TEXT("125.56ms"), Value, Error);
	TestEqual(TEXT("ValueInMiliseconds"), 0.12556, Value);

	Converter.Convert(TEXT("14.2 µs"), Value, Error);
	TestEqual(TEXT("ValueInMicroseconds"), 14.2 * 1.e-6, Value);

	Converter.Convert(TEXT("0.72us"), Value, Error);
	TestEqual(TEXT("ValueInMicroseconds2"), 0.72 * 1.e-6, Value); 

	Converter.Convert(TEXT("3ns"), Value, Error);
	TestEqual(TEXT("ValueInNanoseconds"), 3 * 1.e-9, Value);

	Converter.Convert(TEXT("17ns"), Value, Error);
	TestEqual(TEXT("ValueInPicoseconds"), 17 * 1.e-12, Value);

	Converter.Convert(TEXT("0.5m"), Value, Error);
	TestEqual(TEXT("ValueInMinutes"), 30.0, Value);

	Converter.Convert(TEXT("1.1h"), Value, Error);
	TestEqual(TEXT("ValueInHours"), 3960.0, Value);

	Converter.Convert(TEXT("2d"), Value, Error);
	TestEqual(TEXT("ValueInHours"), 2 * 60 * 60 * 24.0, Value);

	TestFalse(TEXT("Fail1"), Converter.Convert(TEXT("1.23ss"), Value, Error));
	TestFalse(TEXT("Fail2"), Converter.Convert(TEXT("abc"), Value, Error));
	TestFalse(TEXT("FailInvalidChar"), Converter.Convert(TEXT("0,2"), Value, Error));

	return !HasAnyErrors();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
