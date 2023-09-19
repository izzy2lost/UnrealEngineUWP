// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
//awful hack since SlabAllocator is private
#include "Developer/TraceServices/Private/Common/SlabAllocator.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Common/PagedArray.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsCommands.h"
#include "Insights/Tests/InsightsTestUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPagedArrayFilteringTest, "Insights.PagedArrayFiltering", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool operator!=(const FInt32Interval& lhs, const FInt32Interval& rhs)
{
	return lhs.Min != rhs.Min || lhs.Max != rhs.Max;
}

bool FPagedArrayFilteringTest::RunTest(const FString& Parameters)
{
	using namespace TraceServices;
	
	TArray<int32> Integers {0,1,2,3,4,5,6,7,8,9,10};
	TArray<int32> Integer {1};
	
	//first element >= Value
	TestEqual(TEXT("First index of element found in range"), Algo::LowerBound(Integers, 4),4);
	TestEqual(TEXT("First index of element not in range"), Algo::LowerBound(Integers, 100),11);
	//first element > Value
	TestEqual(TEXT("Upper bound value found in range"), Algo::UpperBound(Integers, 4), 5);
	TestEqual(TEXT("Upper bound value not found in range"), Algo::UpperBound(Integers, 100), 11);
	
	TestEqual(TEXT("Upper bound value found in single-item range"), Algo::UpperBound(Integer, 10), 1);

	FSlabAllocator alloc(32 << 20);
	int32 PageSize = 4;

	struct FTimeRegion
	{
		double BeginTime;
		double EndTime;
	};
	
	TPagedArray<FTimeRegion> EmptyLane(alloc, PageSize);

	TPagedArray<FTimeRegion> OneItemLane(alloc, PageSize);
	OneItemLane.EmplaceBack(FTimeRegion{ 1.0, 2.0 });

	TPagedArray<FTimeRegion> TestLane(alloc, PageSize);
	TestLane.EmplaceBack(FTimeRegion{ 1.0, 2.0 });
	TestLane.EmplaceBack(FTimeRegion{ 3.0, 4.0 });
	TestLane.EmplaceBack(FTimeRegion{ 5.0, 6.0 });
	TestLane.EmplaceBack(FTimeRegion{ 7.0, 8.0 });
	TestLane.EmplaceBack(FTimeRegion{ 9.0, 10.0 });
	TestLane.EmplaceBack(FTimeRegion{ 11.0, 12.0 });
	TestLane.EmplaceBack(FTimeRegion{ 13.0, 14.0 });
	
	FInt32Interval Result;
	auto BeginProj = [](const FTimeRegion& r ){ return r.BeginTime;};
	auto EndProj = [](const FTimeRegion& r ){ return r.EndTime;};
	
	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(EmptyLane, -10.0, 0.0,BeginProj, EndProj);
	TestEqual(TEXT("No overlaps for empty range"), Result, FInt32Interval{-1,-1});

	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(OneItemLane, -10.0, 10.0,BeginProj, EndProj);
	TestEqual(TEXT("Overlap is interval is larger than element-range"), Result, {0,0});
	
	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(TestLane, -10.0, 0.0,BeginProj, EndProj);
	TestEqual(TEXT("No overlaps for interval before element-range"), Result, {-1,-1});

	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(TestLane, 100.0, 200.0,BeginProj, EndProj);
	TestEqual(TEXT("No overlaps for interval after element-range"), Result, {-1,-1});

	//partially overlapping begin
	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(TestLane, 0, 5,BeginProj, EndProj);
	TestEqual(TEXT("Overlaps for interval earlier to inside element-range"), Result, {0,2});	
	
	// full inside
	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(TestLane, 4, 8,BeginProj, EndProj);
	TestEqual(TEXT("Overlaps for interval inside element-range"), Result, {2,3});

	//partially overlapping end
	Result = GetElementRangeOverlappingGivenRange<FTimeRegion>(TestLane, 11.3, 100,BeginProj, EndProj);
	TestEqual(TEXT("Overlaps for interval inside to after element-range"), Result, {5,6});

	return !HasAnyErrors();
}

#if !WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FСommandsExportWindowsTest, "Insights.CommandsExport(Windows)", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void VerifyExportedLines(const FString& ExportReportPath, const FString& CmdLogPath, const FString& Elements, FInsightsTestUtils Utils, FAutomationTestBase* Test, const float& Timeout)
{
	float StartTime = FPlatformTime::Seconds();
	bool bLineFound = false;
	FString ExpectedResult;

	while ((FPlatformTime::Seconds() - StartTime) < Timeout)
	{
		FString FileContent;

		if (FFileHelper::LoadFileToString(FileContent, *ExportReportPath))
		{
			TArray<FString> Lines;
			FileContent.ParseIntoArrayLines(Lines);
			ExpectedResult = FString::Printf(TEXT("Exported %d %s to file"), Lines.Num() - 1, *Elements);
			bLineFound = Utils.FileContainsString(CmdLogPath, ExpectedResult, 1.0f);
			if (bLineFound)
			{
				Test->TestTrue(FString::Printf(TEXT("Line '%s' from '%s' should exists in file: '%s'"), *ExpectedResult, *ExportReportPath, *CmdLogPath), bLineFound);
				return;
			}
			else
			{
				FPlatformProcess::Sleep(0.1f);
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.1f);
		}
	}

	Test->AddError(FString::Printf(TEXT("VerifyExportedLines timed out while trying to find line '%s' from '%s'"), *ExpectedResult, *ExportReportPath));
}

bool FСommandsExportWindowsTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FInsightsTestUtils Utils(this);

	const FString StoreDir = InsightsManager->GetStoreDir();
	const FString SourceTracePath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/CommandsExportTest_5.4.utrace");
	const FString SourceExportPath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Rsp/export.rsp");
	const FString StoreTracePath = StoreDir / TEXT("CommandsExportTest_5.4.utrace");
	const FString TestResultsDirPath = TEXT("/TestResults");
	const FString SingleCommandDirPath = TEXT("/TestResults/SingleCommand");
	const FString LogResultExportPath = TEXT("/TestResults/export.rsp");
	const FString CmdLogPath = TEXT("/TestResults/Logs/cmd.log");
	const FString CmdThreadLogPath = TEXT("/TestResults/Logs/cmd_treads.log");
	const FString CmdTimersLogPath = TEXT("/TestResults/Logs/cmd_timers.log");
	const FString CmdTimingEventsLogPath = TEXT("/TestResults/Logs/cmd_timing_events.log");
	const FString CmdTimingEventsNonDefaultLogPath = TEXT("/TestResults/Logs/cmd_timing_non_default.log");
	const FString CmdRegionsLogPath = TEXT("/TestResults/Logs/cmd_regions.log");
	const FString CmdExportLogPath = TEXT("/TestResults/Logs/cmd_export.log");
	const FString ExportThreadsTask = TEXT("TimingInsights.ExportThreads /TestResults/SingleCommand/Threads.csv");
	const FString ExportTimersTask = TEXT("TimingInsights.ExportTimers /TestResults/SingleCommand/Timers.csv");
	const FString ExportTimingEventsTask = TEXT("TimingInsights.ExportTimingEvents /TestResults/SingleCommand/TimingEvents.csv");
	const FString ExportTimingEventsBorderedTask = TEXT("TimingInsights.ExportTimingEvents /TestResults/SingleCommand/TimingEventsNonDefault.CSV  -columns=ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth -threads=GameThread -timers=* -startTime=10 -endTime=20");
	const FString ExportTimerStatisticsTask = TEXT("TimingInsights.ExportTimerStatistics  /TestResults/SingleCommand/*.csv -region=* -threads=GPU");
	const FString ExportThreadsReportPath = TEXT("/TestResults/SingleCommand/Threads.csv");
	const FString ExportTimersReportPath = TEXT("/TestResults/SingleCommand/Timers.csv");
	const FString ExportTimingEventsReportPath = TEXT("/TestResults/SingleCommand/TimingEvents.csv");
	const FString ExportTimingEventsNonDefaultReportPath = TEXT("/TestResults/SingleCommand/TimingEventsNonDefault.csv");
	const FString ExportTimerStatisticsReportPath = TEXT("/TestResults/SingleCommand/TimerStatistics.csv");
	bool bLineFound = false;
	const float Timeout = 150.0;

	UTEST_TRUE("Trace in project exists", PlatformFile.FileExists(*SourceTracePath));
	UTEST_TRUE("Export in project exists", PlatformFile.FileExists(*SourceExportPath));
	UTEST_FALSE("Trace in store should not exists before copy", PlatformFile.FileExists(*StoreTracePath));
	
	PlatformFile.CopyFile(*StoreTracePath, *SourceTracePath);
	UTEST_TRUE(TEXT("Trace in store should exists after copy"), PlatformFile.FileExists(*StoreTracePath));

	if (PlatformFile.DirectoryExists(*TestResultsDirPath))
	{
		AddWarning(TEXT("The TestResults directory already exists. Deleting to avoid undefined behavior"));
		IFileManager::Get().DeleteDirectory(*TestResultsDirPath, false, true);
		float StartTime = FPlatformTime::Seconds();
		while ((FPlatformTime::Seconds() - StartTime) < Timeout)
		{
			if (!PlatformFile.DirectoryExists(*TestResultsDirPath))
			{ 
				break;
			}
			FPlatformProcess::Sleep(0.1f);
		}

		UTEST_FALSE("Previously created TestResults directory is not deleted. Execution of the next steps will cause unstable behavior.", PlatformFile.DirectoryExists(*TestResultsDirPath));
	}

    // ExportThreads
	FString InsightsParameters = FString::Printf(TEXT("-OpenTraceFile=\"%s\" -ABSLOG=\"%s\" -AutoQuit -NoUI  -ExecOnAnalysisCompleteCmd=\"%s\" -log"), *StoreTracePath, *CmdThreadLogPath, *ExportThreadsTask);
	InsightsManager->OpenUnrealInsights(*InsightsParameters);
	
	VerifyExportedLines(ExportThreadsReportPath, CmdThreadLogPath, TEXT("threads"), Utils, this, Timeout);

	// ExportTimers
	InsightsParameters = FString::Printf(TEXT("-OpenTraceFile=\"%s\" -ABSLOG=\"%s\" -AutoQuit -NoUI  -ExecOnAnalysisCompleteCmd=\"%s\" -log"), *StoreTracePath, *CmdTimersLogPath, *ExportTimersTask);
	InsightsManager->OpenUnrealInsights(*InsightsParameters);

	VerifyExportedLines(ExportTimersReportPath, CmdTimersLogPath, TEXT("timers"), Utils, this, Timeout);

	// UI verification cannot be executed in that case. 
	
	// ExportTimingEvents
	InsightsParameters = FString::Printf(TEXT("-OpenTraceFile=\"%s\" -ABSLOG=\"%s\" -AutoQuit -NoUI  -ExecOnAnalysisCompleteCmd=\"%s\" -log"), *StoreTracePath, *CmdTimingEventsLogPath, *ExportTimingEventsTask);
	InsightsManager->OpenUnrealInsights(*InsightsParameters);

	VerifyExportedLines(ExportTimingEventsReportPath, CmdTimingEventsLogPath, TEXT("timing events"), Utils, this, Timeout);

	// ExportTimingEventsNonDefault
	InsightsParameters = FString::Printf(TEXT("-OpenTraceFile=\"%s\" -ABSLOG=\"%s\" -AutoQuit -NoUI  -ExecOnAnalysisCompleteCmd=\"%s\" -log"), *StoreTracePath, *CmdTimingEventsNonDefaultLogPath, *ExportTimingEventsBorderedTask);
	InsightsManager->OpenUnrealInsights(*InsightsParameters);

	const TArray<FString> ExpectedTimingEventsBorderedElements = { TEXT("ThreadId"), TEXT("ThreadName"), TEXT("TimerId"), TEXT("TimerName"), TEXT("StartTime"), TEXT("EndTime"), TEXT("Duration"), TEXT("Depth"),
		TEXT("2,GameThread,1,FEngineLoop::PreInitPreStartupScreen")
	};

	for (int i = 0; i < ExpectedTimingEventsBorderedElements.Num(); i++)
	{
		bLineFound = Utils.FileContainsString(ExportTimingEventsNonDefaultReportPath, ExpectedTimingEventsBorderedElements[i], Timeout);
		TestTrue(FString::Printf(TEXT("Line '%s' should exists in file: '%s'"), *ExpectedTimingEventsBorderedElements[i], *ExportTimingEventsNonDefaultReportPath), bLineFound);
	}

	VerifyExportedLines(ExportTimingEventsNonDefaultReportPath, CmdTimingEventsNonDefaultLogPath, TEXT("timing events"), Utils, this, Timeout);

	// Export regions
	InsightsParameters = FString::Printf(TEXT("-OpenTraceFile=\"%s\" -ABSLOG=\"%s\" -AutoQuit -NoUI  -ExecOnAnalysisCompleteCmd=\"%s\" -log"), *StoreTracePath, *CmdRegionsLogPath, *ExportTimerStatisticsTask);
	InsightsManager->OpenUnrealInsights(*InsightsParameters);

	bLineFound = Utils.FileContainsString(CmdRegionsLogPath, TEXT("Exported timing statistics for 10 regions"), Timeout);
	TestTrue(FString::Printf(TEXT("Line '%s' should exists in file: '%s'"), TEXT("Exported timing statistics for 10 regions"), *CmdRegionsLogPath), bLineFound);

	TArray<FString> RegionFiles;
	FString FilePattern = TEXT("*.csv"); // Change the pattern to filter .csv files
	IFileManager::Get().FindFiles(RegionFiles, *FPaths::Combine(*SingleCommandDirPath, *FilePattern), true, false);
	TestEqual(FString::Printf(TEXT("Should be 14 csv files but found '%d'"), RegionFiles.Num()), RegionFiles.Num(), 14);

	// Export.rsp
	PlatformFile.CopyFile(*LogResultExportPath, *SourceExportPath);
	TestTrue("Rsp in log directory should exists after copy", PlatformFile.FileExists(*LogResultExportPath));
 
	InsightsParameters = FString::Printf(TEXT("-OpenTraceFile=\"%s\" -ABSLOG=\"%s\" -AutoQuit -NoUI  -ExecOnAnalysisCompleteCmd=\"@=/TestResults/export.rsp\" -log"), *StoreTracePath, *CmdExportLogPath);
	InsightsManager->OpenUnrealInsights(*InsightsParameters);

	const TArray<FString> ExpectedThreadsElementsRsp = { TEXT("/TestResults/RSPtest/CSV/Threads_rsp.csv"),
		TEXT("/TestResults/RSPtest/TSV/Threads_rsp.tsv"),
		TEXT("/TestResults/RSPtest/TXT/Threads_rsp.txt"),
	};

	for (int i = 0; i < ExpectedThreadsElementsRsp.Num(); i++)
	{
		bLineFound = Utils.FileContainsString(CmdExportLogPath, ExpectedThreadsElementsRsp[i], Timeout);
		TestTrue(FString::Printf(TEXT("Line '%s' should exists in file: '%s'"), *ExpectedThreadsElementsRsp[i], *CmdExportLogPath), bLineFound);

		VerifyExportedLines(ExpectedThreadsElementsRsp[i], CmdExportLogPath, TEXT("threads"), Utils, this, Timeout);
	}

	const TArray<FString> ExpectedTimersElementsRsp = { TEXT("/TestResults/RSPtest/CSV/Timers_rsp.csv"),
		TEXT("/TestResults/RSPtest/TSV/Timers_rsp.tsv"),
		TEXT("/TestResults/RSPtest/TXT/Timers_rsp.txt"),
	};

	for (int i = 0; i < ExpectedTimersElementsRsp.Num(); i++)
	{
		bLineFound = Utils.FileContainsString(CmdExportLogPath, ExpectedTimersElementsRsp[i], Timeout);
		TestTrue(FString::Printf(TEXT("Line '%s' should exists in file: '%s'"), *ExpectedTimersElementsRsp[i], *CmdExportLogPath), bLineFound);

		VerifyExportedLines(ExpectedTimersElementsRsp[i], CmdExportLogPath, TEXT("timers"), Utils, this, Timeout);
	}

	const TArray<FString> ExpectedTimingEventsRsp = { TEXT("/TestResults/RSPtest/CSV/TimingEvents_rsp.csv"),
		TEXT("/TestResults/RSPtest/TSV/TimingEvents_rsp.tsv"),
		TEXT("/TestResults/RSPtest/TXT/TimingEvents_rsp.txt"),
	};

	for (int i = 0; i < ExpectedTimingEventsRsp.Num(); i++)
	{
		bLineFound = Utils.FileContainsString(CmdExportLogPath, ExpectedTimingEventsRsp[i], Timeout);
		TestTrue(FString::Printf(TEXT("Line '%s' should exists in file: '%s'"), *ExpectedTimingEventsRsp[i], *CmdExportLogPath), bLineFound);

		VerifyExportedLines(ExpectedTimingEventsRsp[i], CmdExportLogPath, TEXT("timing events"), Utils, this, Timeout);
	}
	
	IFileManager::Get().Delete(*StoreTracePath);

	return true;
}

#endif // !WITH_EDITOR