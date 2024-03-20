// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

#define TEST_NAME_ROOT "System.Engine.WorldPartition"

namespace WorldPartitionTests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPartitionStaticSpatialIndexTest, TEST_NAME_ROOT ".StaticSpatialIndex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#if WITH_EDITOR
	template <class Class>
	void PerformTests(FWorldPartitionStaticSpatialIndexTest* Test, const TCHAR* Name, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, TArray<int32>& Results)
	{
		Class SpatialIndex;
		SpatialIndex.Init(Elements);

		Results.Reserve(Tests.Num());

		const double StartTime = FPlatformTime::Seconds();

		for (int32 ListNumTests = 0; ListNumTests < Tests.Num(); ListNumTests++)
		{
			SpatialIndex.ForEachIntersectingElement(Tests[ListNumTests], [&Results](const int32& Value) { Results.Add(Value); });
		}

		const double RunTime = FPlatformTime::Seconds() - StartTime;
		Test->AddInfo(FString::Printf(TEXT("%s: %d tests in %s (%.2f/s, %s)"), Name, Tests.Num(), *FPlatformTime::PrettyTime(RunTime), Tests.Num() / RunTime, *FGenericPlatformMemory::PrettyMemory(SpatialIndex.GetAllocatedSize())));
	}


	template <int32 MaxNumElementsPerNode, int32 MaxNumElementsPerLeaf>
	void PerformMinXTest(FWorldPartitionStaticSpatialIndexTest* Test, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, const TArray<int32>& ReferenceResults)
	{
		TArray<int32> FRTreeMinXResults;
		FRTreeMinXResults.Reserve(ReferenceResults.Num());
		FString TestName = FString::Printf(TEXT("TStaticSpatialIndexRTree(minx-%d-%d)"), MaxNumElementsPerNode, MaxNumElementsPerLeaf);
		PerformTests<TStaticSpatialIndexRTree<int32, FStaticSpatialIndex::FNodeSorterMinX>>(Test, *TestName, Elements, Tests, FRTreeMinXResults);
		FRTreeMinXResults.Sort();
		Test->TestTrue(TestName, FRTreeMinXResults == ReferenceResults);
	}

	void PerformMinXTests(FWorldPartitionStaticSpatialIndexTest* Test, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, const TArray<int32>& ReferenceResults)
	{
		PerformMinXTest<16, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<16, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<16, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<16, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<64, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<64, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<64, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<64, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<256, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<256, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<256, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<256, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<1024, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<1024, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<1024, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMinXTest<1024, 1024>(Test, Elements, Tests, ReferenceResults);
	}

	template <int32 BucketSize, int32 MaxNumElementsPerNode, int32 MaxNumElementsPerLeaf>
	void PerformMortonTest(FWorldPartitionStaticSpatialIndexTest* Test, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, const TArray<int32>& ReferenceResults)
	{
		TArray<int32> FRTreeMortonResults;
		FRTreeMortonResults.Reserve(ReferenceResults.Num());
		FString TestName = FString::Printf(TEXT("TStaticSpatialIndexRTree(morton-%dk-%d-%d)"), BucketSize >> 10, MaxNumElementsPerNode, MaxNumElementsPerLeaf);
		PerformTests<TStaticSpatialIndexRTree<int32, FStaticSpatialIndex::TNodeSorterMorton<BucketSize>>>(Test, *TestName, Elements, Tests, FRTreeMortonResults);
		FRTreeMortonResults.Sort();
		Test->TestTrue(TestName, FRTreeMortonResults == ReferenceResults);
	}

	template <int32 BucketSize>
	void PerformMortonTests(FWorldPartitionStaticSpatialIndexTest* Test, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, const TArray<int32>& ReferenceResults)
	{
		PerformMortonTest<BucketSize, 16, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 16, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 16, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 16, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 64, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 64, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 64, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 64, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 256, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 256, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 256, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 256, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 1024, 16>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 1024, 64>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 1024, 256>(Test, Elements, Tests, ReferenceResults);
		PerformMortonTest<BucketSize, 1024, 1024>(Test, Elements, Tests, ReferenceResults);
	}

	template <int32 BucketSize, int32 MaxNumElementsPerNode, int32 MaxNumElementsPerLeaf>
	void PerformHilbertTest(FWorldPartitionStaticSpatialIndexTest* Test, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, const TArray<int32>& ReferenceResults)
	{
		TArray<int32> FRTreeHilbertResults;
		FRTreeHilbertResults.Reserve(ReferenceResults.Num());
		FString TestName = FString::Printf(TEXT("TStaticSpatialIndexRTree(hilbert-%dk-%d-%d)"), BucketSize >> 10, MaxNumElementsPerNode, MaxNumElementsPerLeaf);
		PerformTests<TStaticSpatialIndexRTree<int32, FStaticSpatialIndex::TNodeSorterHilbert<BucketSize>>>(Test, *TestName, Elements, Tests, FRTreeHilbertResults);
		FRTreeHilbertResults.Sort();
		Test->TestTrue(TestName, FRTreeHilbertResults == ReferenceResults);
	}

	template <int32 BucketSize>
	void PerformHilbertTests(FWorldPartitionStaticSpatialIndexTest* Test, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, const TArray<int32>& ReferenceResults)
	{
		PerformHilbertTest<BucketSize, 16, 16>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 16, 64>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 16, 256>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 16, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 64, 16>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 64, 64>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 64, 256>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 64, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 256, 16>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 256, 64>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 256, 256>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 256, 1024>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 1024, 16>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 1024, 64>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 1024, 256>(Test, Elements, Tests, ReferenceResults);
		PerformHilbertTest<BucketSize, 1024, 1024>(Test, Elements, Tests, ReferenceResults);
	}
#endif

	bool FWorldPartitionStaticSpatialIndexTest::RunTest(const FString& Parameters)
	{
#if WITH_EDITOR
		const int32 NumBoxes = 100000;
		const int32 NumTests = 10000;

		TArray<TPair<FBox, int32>> Elements;
		Elements.Reserve(NumBoxes);
		for (int32 i=0; i<NumBoxes; i++)
		{
			const FSphere Sphere(FMath::VRand() * 10000000, FMath::RandRange(1, 100000));
			Elements.Emplace(FBoxSphereBounds(Sphere).GetBox(), i);
		}

		TArray<FSphere> Tests;
		Tests.Reserve(NumTests);
		for (int32 i=0; i<NumTests; i++)
		{
			const FSphere Sphere(FMath::VRand() * 10000000, FMath::RandRange(1, 100000));
			Tests.Add(Sphere);
		}

		TArray<int32> ListResults;
		PerformTests<TStaticSpatialIndexList<int32, FStaticSpatialIndex::FNodeSorterNoSort>>(this, TEXT("TStaticSpatialIndexList"), Elements, Tests, ListResults);
		ListResults.Sort();

		PerformMinXTests(this, Elements, Tests, ListResults);
		PerformMinXTests(this, Elements, Tests, ListResults);
		PerformMinXTests(this, Elements, Tests, ListResults);
		PerformMinXTests(this, Elements, Tests, ListResults);

		PerformMortonTests<4096>(this, Elements, Tests, ListResults);
		PerformMortonTests<16384>(this, Elements, Tests, ListResults);
		PerformMortonTests<65536>(this, Elements, Tests, ListResults);
		PerformMortonTests<262144>(this, Elements, Tests, ListResults);

		PerformHilbertTests<4096>(this, Elements, Tests, ListResults);
		PerformHilbertTests<16384>(this, Elements, Tests, ListResults);
		PerformHilbertTests<65536>(this, Elements, Tests, ListResults);
		PerformHilbertTests<262144>(this, Elements, Tests, ListResults);
#endif
		return true;
	}
}

#undef TEST_NAME_ROOT

#endif