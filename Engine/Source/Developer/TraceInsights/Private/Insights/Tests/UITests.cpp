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

// TestRail: C28843555
void FAutomaticRenamingAndDeletingOfSymbolCacheFilesInsightsTest::Define()
{
	BeforeEach([this]() {
		if (IAutomationDriverModule::Get().IsEnabled())
		{
			IAutomationDriverModule::Get().Disable();
		}

		IAutomationDriverModule::Get().Enable();

		Driver = IAutomationDriverModule::Get().CreateDriver();

		});
	Describe("Make sure the user can rename and delete trace with automatic cache deletion", [this]()
		{

			It("Copy, rename and delete", EAsyncExecution::ThreadPool, [this]()
				{
					FDriverElementRef MinimizeWindowButton = Driver->FindElements(By::Id("launcher-minimizeWindowButton"))->GetElements()[0];
					MinimizeWindowButton->Click();

					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
					TestTrue("Insigts manager should not be null", InsightsManager.IsValid());
					InsightsManager->GetTraceStoreWindow()->SetDeleteTraceConfirmationWindowVisibility(false);

					FString StoreDir = InsightsManager->GetStoreDir();
					FString ProjectDir = FPaths::ProjectDir();

					const FString SourceTestTracePath = ProjectDir + TEXT("SourceAssets/UTraces/Test.utrace");
					const FString SourceTestCachePath = ProjectDir + TEXT("SourceAssets/UCaches/Test.ucache");

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
					auto TraceWaiter = [Driver = Driver](void) -> bool {
						return Driver->FindElements(By::Id("TraceList"))->GetElements()[0]->GetText().ToString() == TEXT("Test");
					};
					Driver->Wait(Until::Condition(TraceWaiter, FWaitTimeout::InSeconds(3)));
					FDriverElementRef TraceElement = Driver->FindElements(By::Id("TraceList"))->GetElements()[0];

					FDriverSequenceRef Sequence = Driver->CreateSequence();
					Sequence->Actions()
						.Click(TraceElement)
						.Type(EKeys::F2)
						.Type(TEXT("UcacheRenaming"))
						.Type(EKeys::Enter);

					TestTrue("Trace renamed", Sequence->Perform());

					TestTrue("Renamed trace should exist", PlatformFile.FileExists(*StoreTestTracePath));
					TestTrue("Renamed cache should exist", PlatformFile.FileExists(*StoreTestCachePath));

					// Delete
					FDriverElementRef OpenTraceButton = Driver->FindElement(By::Id("OpenTraceButton"));
					Driver->Wait(Until::ElementIsInteractable(OpenTraceButton, FWaitTimeout::InSeconds(5)));

					TraceElement = Driver->FindElements(By::Id("TraceList"))->GetElements()[0];
					TraceElement->Type(EKeys::Delete);

					TestFalse("Renamed trace should be deleted", PlatformFile.FileExists(*StoreTestTracePath));
					TestFalse("Renamed cache should be deleted", PlatformFile.FileExists(*StoreTestCachePath));
				});
		});

	AfterEach([this]() {
		Driver.Reset();
		IAutomationDriverModule::Get().Disable();
	});
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
