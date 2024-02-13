// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/TestHarnessAdapter.h"
#endif	// WITH_TESTS

#include "HAL/IConsoleManager.h"
#include "WorldMetrics.h"
#include "WorldMetricsLog.h"
#include "WorldMetricsSubsystem.h"
#include "WorldMetricsTestTypes.h"
#include "WorldMetricsTestUtil.h"

//---------------------------------------------------------------------------------------------------------------------
// UE::WorldMetrics::Private
//---------------------------------------------------------------------------------------------------------------------
namespace UE::WorldMetrics::Private
{

void TestAll(UWorld* World);

static FAutoConsoleCommandWithWorldAndArgs CmdWorldMetricsSelfTest(
	TEXT("WorldMetrics.SelfTest"),
	TEXT("Toggles the World Metrics Subsystem self-test."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
														  { TestAll(World); }),
	ECVF_Default);

static const TSubclassOf<UMockWorldMetricBase> MockMetricClasses[] = {
	UMockWorldMetricA::StaticClass(), UMockWorldMetricB::StaticClass(), UMockWorldMetricC::StaticClass(),
	UMockWorldMetricD::StaticClass(), UMockWorldMetricE::StaticClass(), UMockWorldMetricF::StaticClass(),
	UMockWorldMetricG::StaticClass(), UMockWorldMetricH::StaticClass(), UMockWorldMetricI::StaticClass(),
	UMockWorldMetricJ::StaticClass()};
static constexpr int32 NumMockMetricClasses = sizeof(MockMetricClasses) / sizeof(MockMetricClasses[0]);

static const TSubclassOf<UMockWorldMetricsExtensionBase> MockExtensionClasses[] = {
	UMockWorldMetricsExtensionA::StaticClass(), UMockWorldMetricsExtensionB::StaticClass(),
	UMockWorldMetricsExtensionC::StaticClass(), UMockWorldMetricsExtensionD::StaticClass(),
	UMockWorldMetricsExtensionE::StaticClass(), UMockWorldMetricsExtensionF::StaticClass(),
	UMockWorldMetricsExtensionG::StaticClass(), UMockWorldMetricsExtensionH::StaticClass(),
	UMockWorldMetricsExtensionI::StaticClass(), UMockWorldMetricsExtensionJ::StaticClass()};

static constexpr int32 NumMockExtensionClasses = sizeof(MockExtensionClasses) / sizeof(MockExtensionClasses[0]);

TArray<TSubclassOf<UMockWorldMetricBase>> AddMockMetrics(UWorld* World, int32 Count, bool bRandomized = false)
{
	TArray<TSubclassOf<UMockWorldMetricBase>> Result;

	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	if (UNLIKELY(!Subsystem))
	{
		return Result;
	}
	Count = FMath::Min(Count, NumMockMetricClasses);
	Result.Reserve(Count);

	const TArray<int32> ClassIndices =
		bRandomized ? MakeRandomSubset<int32>(MakeIndexArray(NumMockMetricClasses), Count) : MakeIndexArray(Count);

	for (int32 ClassIndex : ClassIndices)
	{
		if (Subsystem->AddMetric(MockMetricClasses[ClassIndex]))
		{
			Result.Emplace(MockMetricClasses[ClassIndex]);
		}
	}
	return Result;
}

TArray<UMockWorldMetricBase*> GetMockMetrics(
	UWorld* World,
	TArrayView<TSubclassOf<UMockWorldMetricBase>> InMockMetricClasses)
{
	TArray<UMockWorldMetricBase*> Result;

	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	if (UNLIKELY(!Subsystem))
	{
		return Result;
	}

	Result.Reserve(InMockMetricClasses.Num());
	for (const TSubclassOf<UMockWorldMetricBase>& MetricClass : InMockMetricClasses)
	{
		if (UWorldMetricInterface* Metric = Subsystem->GetMetric(MetricClass))
		{
			Result.Emplace(static_cast<UMockWorldMetricBase*>(Metric));
		}
	}
	return Result;
}

int32 RemoveMockMetrics(
	UWorld* World,
	TArrayView<TSubclassOf<UMockWorldMetricBase>> InMockMetricClasses,
	bool bRandomized = false)
{
	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	if (UNLIKELY(!Subsystem))
	{
		return 0;
	}

	TArray<int32> ClassIndices = MakeIndexArray(InMockMetricClasses.Num());
	if (bRandomized)
	{
		Shuffle(ClassIndices);
	}

	int32 Result = 0;
	for (int32 ClassIndex : ClassIndices)
	{
		if (Subsystem->RemoveMetric(InMockMetricClasses[ClassIndex]))
		{
			++Result;
		}
	}
	return Result;
}

TArray<TSubclassOf<UMockWorldMetricsExtensionBase>>
AcquireMockMetricExtensions(UMockWorldMetricBase* Owner, int32 Count, bool bRandomized = false)
{
	TArray<TSubclassOf<UMockWorldMetricsExtensionBase>> Result;

	if (UNLIKELY(!Owner))
	{
		return Result;
	}

	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(Owner->GetWorld());
	if (UNLIKELY(!Subsystem))
	{
		return Result;
	}

	Count = FMath::Min(Count, NumMockExtensionClasses);
	Result.Reserve(Count);

	const TArray<int32> ClassIndices =
		bRandomized ? MakeRandomSubset<int32>(MakeIndexArray(NumMockExtensionClasses), Count) : MakeIndexArray(Count);

	for (int32 ClassIndex : ClassIndices)
	{
		if (Subsystem->AcquireExtension(Owner, MockExtensionClasses[ClassIndex]))
		{
			Result.Emplace(MockExtensionClasses[ClassIndex]);
		}
	}
	return Result;
}

int32 ReleaseMockMetricExtensions(
	UMockWorldMetricBase* Owner,
	TArrayView<TSubclassOf<UMockWorldMetricsExtensionBase>> InMockExtensionClasses,
	bool bRandomized = false)
{
	if (UNLIKELY(!Owner))
	{
		return 0;
	}

	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(Owner->GetWorld());
	if (UNLIKELY(!Subsystem))
	{
		return 0;
	}

	TArray<int32> ClassIndices = MakeIndexArray(InMockExtensionClasses.Num());
	if (bRandomized)
	{
		Shuffle(ClassIndices);
	}

	int32 Result = 0;
	for (int32 ClassIndex : ClassIndices)
	{
		if (Subsystem->ReleaseExtension(Owner, InMockExtensionClasses[ClassIndex]))
		{
			++Result;
		}
	}
	return Result;
}

void TestZeroState(UWorld* World)
{
	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem is valid after GetSubsystem."), Subsystem);

	REQUIRE_MESSAGE(TEXT("Unexpected existing metrics in WorldMetricsSubsystem"), !Subsystem->HasAnyMetric());
	Subsystem->Enable(true);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem shouldn't be enabled if there are no metrics."), !Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should zero metrics."), Subsystem->NumMetrics() == 0);

	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem shouldn't be have any extension."), !Subsystem->HasAnyExtension());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have zero extensions."), Subsystem->NumExtensions() == 0);
}

void TestSingleMetricAddRemove(UWorld* World)
{
	TestZeroState(World);

	// UE::WorldMetrics API
	{
		REQUIRE_MESSAGE(TEXT("AddMetric failed"), UE::WorldMetrics::AddMetric<UMockWorldMetricA>(World));

		UMockWorldMetricA* MetricA = UE::WorldMetrics::GetMetric<UMockWorldMetricA>(World);
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MetricA->InitializeCount == 1);

		const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
		REQUIRE_MESSAGE(
			TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have metrics."), Subsystem->HasAnyMetric());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one metric."), Subsystem->NumMetrics() == 1);

		REQUIRE_MESSAGE(TEXT("Unexpected AddMetric"), !UE::WorldMetrics::AddMetric<UMockWorldMetricA>(World));
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one metric."), Subsystem->NumMetrics() == 1);

		REQUIRE_MESSAGE(TEXT("RemoveMetric failed."), UE::WorldMetrics::RemoveMetric<UMockWorldMetricA>(World));
		REQUIRE_MESSAGE(TEXT("Test Metric deinitialization failed after release."), MetricA->DeinitializeCount == 1);

		TestZeroState(World);

		REQUIRE_MESSAGE(TEXT("Unexpected RemoveMetric."), !UE::WorldMetrics::RemoveMetric<UMockWorldMetricA>(World));

		TestZeroState(World);
	}

	// World Metrics Subsystem API
	{
		UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);

		REQUIRE_MESSAGE(TEXT("AddMetric failed"), Subsystem->AddMetric<UMockWorldMetricA>());

		UMockWorldMetricA* MetricA = Subsystem->GetMetric<UMockWorldMetricA>();
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MetricA->InitializeCount == 1);

		REQUIRE_MESSAGE(
			TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have metrics."), Subsystem->HasAnyMetric());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one metric."), Subsystem->NumMetrics() == 1);

		REQUIRE_MESSAGE(TEXT("Unexpected AddMetric"), !Subsystem->AddMetric<UMockWorldMetricA>());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one metric."), Subsystem->NumMetrics() == 1);

		REQUIRE_MESSAGE(TEXT("RemoveMetric failed."), Subsystem->RemoveMetric<UMockWorldMetricA>());
		REQUIRE_MESSAGE(TEXT("Test Metric deinitialization failed after release."), MetricA->DeinitializeCount == 1);

		TestZeroState(World);

		REQUIRE_MESSAGE(TEXT("Unexpected RemoveMetric."), !Subsystem->RemoveMetric<UMockWorldMetricA>());

		TestZeroState(World);
	}
}

void TestMultipleMetricsAddRemove(UWorld* World, bool bRandomized)
{
	TestZeroState(World);

	constexpr int32 NumMetrics = 5;

	auto MetricClasses = Private::AddMockMetrics(World, NumMetrics, bRandomized);

	const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyMetric());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one extension."), Subsystem->NumMetrics() == NumMetrics);

	TArray<UMockWorldMetricBase*> MockMetrics = Private::GetMockMetrics(World, MetricClasses);
	MockMetrics.Reserve(MetricClasses.Num());
	for (const UMockWorldMetricBase* MockMetric : MockMetrics)
	{
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MockMetric->InitializeCount == 1);
	}

	const int32 NumMetricsRemoved = Private::RemoveMockMetrics(World, MetricClasses, bRandomized);
	REQUIRE_MESSAGE(TEXT("Unexpected metric remove count"), NumMetricsRemoved == NumMetrics);
	for (const UMockWorldMetricBase* Metric : MockMetrics)
	{
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), Metric->DeinitializeCount == 1);
	}

	TestZeroState(World);
}

void TestSingleMetricSingleExtensionAcquireRelease(UWorld* World)
{
	TestZeroState(World);

	REQUIRE_MESSAGE(TEXT("AddMetric failed"), UE::WorldMetrics::AddMetric<UMockWorldMetricA>(World));

	UMockWorldMetricA* MetricA = UE::WorldMetrics::GetMetric<UMockWorldMetricA>(World);
	REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MetricA->InitializeCount == 1);

	// UE::WorldMetrics API
	{
		UMockWorldMetricsExtensionA* ExtensionA =
			UE::WorldMetrics::AcquireExtension<UMockWorldMetricsExtensionA>(MetricA);

		REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionA);
		REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->InitializeCount == 1);

		const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
		REQUIRE_MESSAGE(
			TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyExtension());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one extension."), Subsystem->NumExtensions() == 1);

		REQUIRE_MESSAGE(
			TEXT("Test Extension release failed."),
			UE::WorldMetrics::ReleaseExtension<UMockWorldMetricsExtensionA>(MetricA));
		REQUIRE_MESSAGE(
			TEXT("Test Extension deinitialization failed after release."), ExtensionA->DeinitializeCount == 1);
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem shouldn't be have any extension."), !Subsystem->HasAnyExtension());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have zero extensions."), Subsystem->NumExtensions() == 0);
	}

	// World Metrics Subsystem API
	{
		UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);

		UMockWorldMetricsExtensionA* ExtensionA = Subsystem->AcquireExtension<UMockWorldMetricsExtensionA>(MetricA);
		REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionA);
		REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->InitializeCount == 1);

		REQUIRE_MESSAGE(
			TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyExtension());
		REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one extension."), Subsystem->NumExtensions() == 1);

		REQUIRE_MESSAGE(
			TEXT("Test Extension release failed."), Subsystem->ReleaseExtension<UMockWorldMetricsExtensionA>(MetricA));
		REQUIRE_MESSAGE(
			TEXT("Test Extension deinitialization failed after release."), ExtensionA->DeinitializeCount == 1);
	}

	UE::WorldMetrics::RemoveMetric<UMockWorldMetricA>(World);

	TestZeroState(World);
}

void TestMultipleMetricsSingleExtensionAcquireRelease(UWorld* World, bool bRandomized)
{
	TestZeroState(World);

	constexpr int32 NumMetrics = 5;

	auto MetricClasses = Private::AddMockMetrics(World, NumMetrics, bRandomized);
	TArray<UMockWorldMetricBase*> MockMetrics = Private::GetMockMetrics(World, MetricClasses);
	REQUIRE_MESSAGE(TEXT("Unexpected empty metrics list."), !MockMetrics.IsEmpty());

	UMockWorldMetricsExtensionA* ExtensionA =
		UE::WorldMetrics::AcquireExtension<UMockWorldMetricsExtensionA>(MockMetrics[0]);

	// Acquire extension loop
	{
		int32 CheckCount = 1;
		for (UMockWorldMetricBase* MockMetric : MockMetrics)
		{
			UE::WorldMetrics::AcquireExtension<UMockWorldMetricsExtensionA>(MockMetric);
			REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionA);
			REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->InitializeCount == 1);
			REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->OnAcquireCount == ++CheckCount);
		}
	}
	// Release extension loop
	{
		if (bRandomized)
		{
			Shuffle(MockMetrics);
		}

		int32 CheckCount = 0;
		for (UMockWorldMetricBase* MockMetric : MockMetrics)
		{
			REQUIRE_MESSAGE(TEXT("Test Extension unexpected deinitialization"), ExtensionA->DeinitializeCount == 0);
			UE::WorldMetrics::ReleaseExtension<UMockWorldMetricsExtensionA>(MockMetric);

			REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionA);
			REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->InitializeCount == 1);
			REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->OnReleaseCount == ++CheckCount);
		}
		REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->DeinitializeCount == 1);
	}

	Private::RemoveMockMetrics(World, MetricClasses, bRandomized);

	TestZeroState(World);
}

void TestMultipleMetricsMultipleExtensionAcquireRelease(UWorld* World, bool bRandomized)
{
	TestZeroState(World);

	constexpr int32 NumMetrics = 5;
	constexpr int32 NumExtension = 5;

	auto MetricClasses = Private::AddMockMetrics(World, NumMetrics, bRandomized);
	TArray<UMockWorldMetricBase*> MockMetrics = Private::GetMockMetrics(World, MetricClasses);
	REQUIRE_MESSAGE(TEXT("Unexpected empty metrics list."), !MockMetrics.IsEmpty());

	TArray<TArray<TSubclassOf<UMockWorldMetricsExtensionBase>>> Extensions;
	Extensions.Reserve(NumMetrics);

	// Acquire extensions
	for (UMockWorldMetricBase* MockMetric : MockMetrics)
	{
		Extensions.Emplace(Private::AcquireMockMetricExtensions(MockMetric, NumExtension, bRandomized));
	}

	const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyExtension());

	// Release extensions
	for (int32 MetricIndex = 0; MetricIndex < NumMetrics; ++MetricIndex)
	{
		Private::ReleaseMockMetricExtensions(MockMetrics[MetricIndex], Extensions[MetricIndex], bRandomized);
	}

	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem shouldn't be have any extension."), !Subsystem->HasAnyExtension());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have zero extensions."), Subsystem->NumExtensions() == 0);

	Private::RemoveMockMetrics(World, MetricClasses, bRandomized);

	TestZeroState(World);
}

void TestMultipleMetricsEnable(UWorld* World, bool bRandomized)
{
	TestZeroState(World);

	constexpr int32 NumMetrics = 5;

	auto MetricClasses = Private::AddMockMetrics(World, NumMetrics, bRandomized);

	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyMetric());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one extension."), Subsystem->NumMetrics() == NumMetrics);

	TArray<UMockWorldMetricBase*> MockMetrics = Private::GetMockMetrics(World, MetricClasses);
	MockMetrics.Reserve(MetricClasses.Num());
	for (const UMockWorldMetricBase* MockMetric : MockMetrics)
	{
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MockMetric->InitializeCount == 1);
	}

	Subsystem->Enable(false);
	for (const UMockWorldMetricBase* MockMetric : MockMetrics)
	{
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MockMetric->DeinitializeCount == 1);
	}

	Subsystem->Enable(true);
	for (const UMockWorldMetricBase* MockMetric : MockMetrics)
	{
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), MockMetric->InitializeCount == 2);
	}

	const int32 NumMetricsRemoved = Private::RemoveMockMetrics(World, MetricClasses, bRandomized);
	REQUIRE_MESSAGE(TEXT("Unexpected metric remove count"), NumMetricsRemoved == NumMetrics);
	for (const UMockWorldMetricBase* Metric : MockMetrics)
	{
		REQUIRE_MESSAGE(TEXT("Test Metric initialization failed"), Metric->DeinitializeCount == 2);
	}

	TestZeroState(World);
}

void TestSingleMetricSingleExtensionAutoReleaseOnRemoval(UWorld* World)
{
	TestZeroState(World);

	REQUIRE_MESSAGE(TEXT("AddMetric failed"), UE::WorldMetrics::AddMetric<UMockWorldMetricA>(World));

	UMockWorldMetricA* MetricA = UE::WorldMetrics::GetMetric<UMockWorldMetricA>(World);

	UMockWorldMetricsExtensionA* ExtensionA = UE::WorldMetrics::AcquireExtension<UMockWorldMetricsExtensionA>(MetricA);
	REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionA);
	REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->InitializeCount == 1);

	const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyExtension());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one extension."), Subsystem->NumExtensions() == 1);

	UE::WorldMetrics::RemoveMetric<UMockWorldMetricA>(World);

	REQUIRE_MESSAGE(TEXT("Test Extension deinitialization failed after release."), ExtensionA->DeinitializeCount == 1);
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem shouldn't have any extensions."), !Subsystem->HasAnyExtension());

	TestZeroState(World);
}

void TestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval(UWorld* World, bool bRandomized)
{
	TestZeroState(World);

	constexpr int32 NumMetrics = 5;
	constexpr int32 NumExtension = 5;

	auto MetricClasses = Private::AddMockMetrics(World, NumMetrics, bRandomized);
	TArray<UMockWorldMetricBase*> MockMetrics = Private::GetMockMetrics(World, MetricClasses);
	REQUIRE_MESSAGE(TEXT("Unexpected empty metrics list."), !MockMetrics.IsEmpty());

	// Acquire extensions
	for (UMockWorldMetricBase* MockMetric : MockMetrics)
	{
		Private::AcquireMockMetricExtensions(MockMetric, NumExtension, bRandomized);
	}

	const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyExtension());

	Private::RemoveMockMetrics(World, MetricClasses, bRandomized);

	TestZeroState(World);
}

void TestSingleMetricOrphanExtensionAutoRemoval(UWorld* World)
{
	TestZeroState(World);

	REQUIRE_MESSAGE(TEXT("AddMetric failed"), UE::WorldMetrics::AddMetric<UMockWorldMetricA>(World));

	UMockWorldMetricA* MetricA = UE::WorldMetrics::GetMetric<UMockWorldMetricA>(World);

	UMockWorldMetricsExtensionA* ExtensionA = UE::WorldMetrics::AcquireExtension<UMockWorldMetricsExtensionA>(MetricA);
	REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionA);
	REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionA->InitializeCount == 1);

	// Extension dependency: ExtensionB requires ExtensionA
	UMockWorldMetricsExtensionB* ExtensionB = UE::WorldMetrics::AcquireExtension<UMockWorldMetricsExtensionB>(ExtensionA);
	REQUIRE_MESSAGE(TEXT("Acquire Test Extension failed."), ExtensionB);
	REQUIRE_MESSAGE(TEXT("Test Extension initialization failed"), ExtensionB->InitializeCount == 1);

	const UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(
		TEXT("WorldMetricsSubsystem should be enabled by default if there are metrics."), Subsystem->IsEnabled());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have extensions."), Subsystem->HasAnyExtension());
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem should have one extension."), Subsystem->NumExtensions() == 2);

	UE::WorldMetrics::RemoveMetric<UMockWorldMetricA>(World);

	REQUIRE_MESSAGE(TEXT("Test Extension deinitialization failed after release."), ExtensionA->DeinitializeCount == 1);
	REQUIRE_MESSAGE(TEXT("Test Extension deinitialization failed after release."), ExtensionB->DeinitializeCount == 1);
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem shouldn't have any extensions."), !Subsystem->HasAnyExtension());

	TestZeroState(World);
}

void TestAll(UWorld* World)
{
	UWorldMetricsSubsystem* Subsystem = UE::WorldMetrics::GetSubsystem(World);
	REQUIRE_MESSAGE(TEXT("WorldMetricsSubsystem is valid after GetSubsystem."), Subsystem);

	Subsystem->Clear();

	TestZeroState(World);

	// Metrics
	TestSingleMetricAddRemove(World);
	TestMultipleMetricsAddRemove(World, false);
	TestMultipleMetricsAddRemove(World, true);
	TestMultipleMetricsEnable(World, false);
	TestMultipleMetricsEnable(World, true);

	// Extension
	TestSingleMetricSingleExtensionAcquireRelease(World);
	TestMultipleMetricsSingleExtensionAcquireRelease(World, false);
	TestMultipleMetricsSingleExtensionAcquireRelease(World, true);
	TestMultipleMetricsMultipleExtensionAcquireRelease(World, false);
	TestMultipleMetricsMultipleExtensionAcquireRelease(World, true);

	// Extension auto-removal when no longer acquired
	TestSingleMetricSingleExtensionAutoReleaseOnRemoval(World);
	TestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval(World, false);
	TestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval(World, true);
	TestSingleMetricOrphanExtensionAutoRemoval(World);
}

}  // namespace UE::WorldMetrics::Private

//---------------------------------------------------------------------------------------------------------------------
// WITH_TESTS
//---------------------------------------------------------------------------------------------------------------------

#if WITH_TESTS
namespace UE::WorldMetrics
{
namespace Private
{
static void ScopedWorldTest(EWorldType::Type WorldType, TFunctionRef<void(UWorld* World)> WorldTest)
{
	constexpr bool bInformEngineOfWorld = false;
	UWorld* World = UWorld::CreateWorld(WorldType, bInformEngineOfWorld);
	REQUIRE_MESSAGE(TEXT("World could not be created."), World != nullptr);

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(WorldType);
	WorldContext.SetCurrentWorld(World);

	const FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	WorldTest(World);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(bInformEngineOfWorld);
}
}  // namespace Private

TEST_CASE_NAMED(WorldMetricsTestZeroState, "WorldMetrics::TestZeroState", "[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(EWorldType::Editor, [this](UWorld* World) { Private::TestZeroState(World); });
}

TEST_CASE_NAMED(
	WorldMetricsTestSingleMetricAddRemove,
	"WorldMetrics::TestSingleMetricAddRemove",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(EWorldType::Editor, [this](UWorld* World) { Private::TestSingleMetricAddRemove(World); });
}

TEST_CASE_NAMED(
	WorldMetricsTestMultipleMetricsAddRemove,
	"WorldMetrics::TestMultipleMetricsAddRemove",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World)
		{
			Private::TestMultipleMetricsAddRemove(World, false);
			Private::TestMultipleMetricsAddRemove(World, true);
		});
}

TEST_CASE_NAMED(
	WorldMetricsTestMultipleMetricsEnable,
	"WorldMetrics::TestMultipleMetricsEnable",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World)
		{
			Private::TestMultipleMetricsEnable(World, false);
			Private::TestMultipleMetricsEnable(World, true);
		});
}

TEST_CASE_NAMED(
	WorldMetricsTestSingleMetricSingleExtensionAcquireRelease,
	"WorldMetrics::TestSingleMetricSingleExtensionAcquireRelease",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor, [this](UWorld* World) { Private::TestSingleMetricSingleExtensionAcquireRelease(World); });
}

TEST_CASE_NAMED(
	WorldMetricsTestMultipleMetricsSingleExtensionAcquireRelease,
	"WorldMetrics::TestMultipleMetricsSingleExtensionAcquireRelease",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World)
		{
			Private::TestMultipleMetricsSingleExtensionAcquireRelease(World, false);
			Private::TestMultipleMetricsSingleExtensionAcquireRelease(World, true);
		});
}

TEST_CASE_NAMED(
	WorldMetricsTestMultipleMetricsMultipleExtensionAcquireRelease,
	"WorldMetrics::TestMultipleMetricsMultipleExtensionAcquireRelease",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World)
		{
			Private::TestMultipleMetricsMultipleExtensionAcquireRelease(World, false);
			Private::TestMultipleMetricsMultipleExtensionAcquireRelease(World, true);
		});
}

TEST_CASE_NAMED(
	WorldMetricsTestSingleMetricSingleExtensionAutoReleaseOnRemoval,
	"WorldMetrics::TestSingleMetricSingleExtensionAutoReleaseOnRemoval",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World) { Private::TestSingleMetricSingleExtensionAutoReleaseOnRemoval(World); });
}

TEST_CASE_NAMED(
	WorldMetricsTestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval,
	"WorldMetrics::TestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World)
		{
			Private::TestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval(World, false);
			Private::TestMultipleMetricsMultipleExtensionAutoReleaseOnRemoval(World, true);
		});
}

TEST_CASE_NAMED(
	WorldMetricsTestSingleMetricOrphanExtensionAutoRemoval,
	"WorldMetrics::TestSingleMetricOrphanExtensionAutoRemoval",
	"[WorldMetrics][ClientContext][EngineFilter]")
{
	Private::ScopedWorldTest(
		EWorldType::Editor,
		[this](UWorld* World) { Private::TestSingleMetricOrphanExtensionAutoRemoval(World); });
}

}  // namespace UE::WorldMetrics

#endif	// WITH_TESTS
