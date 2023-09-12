// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"

#include "Elements/PCGCreatePoints.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCreatePointsTest_Basic, FPCGTestBaseClass, "pcg.tests.CreatePoints.Basic", PCGTestsCommon::TestFlags)

bool FPCGCreatePointsTest_Basic::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGCreatePointsSettings>(TestData);
	UPCGCreatePointsSettings* Settings = CastChecked<UPCGCreatePointsSettings>(TestData.Settings);
	
	TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreateRandomPointData(100, 42, false);
	Settings->PointsToCreate = PointData->GetMutablePoints();

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 100);

	for (int i = 0; i < OutPoints.Num(); ++i)
	{
		UTEST_EQUAL(FString::Format(TEXT("InArray[{0}].Seed is equal to OutArray[{0}].Seed"), { i }), OutPoints[i].Seed, Settings->PointsToCreate[i].Seed);
		UTEST_EQUAL(FString::Format(TEXT("InArray[{0}].Density is equal to OutArray[{0}].Density"), { i }), OutPoints[i].Density, Settings->PointsToCreate[i].Density);
		UTEST_EQUAL(FString::Format(TEXT("InArray[{0}].Transform is equal to OutArray[{0}].Transform"), { i }), OutPoints[i].Transform, Settings->PointsToCreate[i].Transform);
	}

	return true;
}

