// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGSortPoints.h"

namespace SortCommonTestData
{
	TUniquePtr<FPCGContext> GenerateTestDataAndRunSort(EPCGSortMethod Method, bool bRandomDensity)
	{
		PCGTestsCommon::FTestData TestData;
		PCGTestsCommon::GenerateSettings<UPCGSortPointsSettings>(TestData);
		UPCGSortPointsSettings* Settings = CastChecked<UPCGSortPointsSettings>(TestData.Settings);
		Settings->InputSource.SetPointProperty(EPCGPointProperties::Density);
		Settings->SortMethod = Method;
		FPCGElementPtr TestElement = TestData.Settings->GetElement();

		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGPinConstants::DefaultInputLabel;
		SourcePin.Data = PCGTestsCommon::CreateRandomPointData(100, 42, bRandomDensity);

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		while (!TestElement->Execute(Context.Get())) {}
	
		return Context;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_Basic, FPCGTestBaseClass, "Plugins.PCG.SortPoints.Basic", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_Basic::RunTest(const FString& Parameters)
{

	TUniquePtr<FPCGContext> Context = SortCommonTestData::GenerateTestDataAndRunSort(EPCGSortMethod::Ascending, false);

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 100);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_Ascending, FPCGTestBaseClass, "Plugins.PCG.SortPoints.Ascending", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_Ascending::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::GenerateTestDataAndRunSort(EPCGSortMethod::Ascending, true);

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	// compare point data
	const FPCGTaggedData& Output = Outputs[0];
	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Output.Data);
	if (OutPointData) 
	{
		const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

		for (int i = 0; i < OutPoints.Num() - 1; ++i)
		{
			UTEST_TRUE(FString::Format(TEXT("{0} is less than/equal to {1}"), { OutPoints[i].Density, OutPoints[i + 1].Density }), OutPoints[i].Density <= OutPoints[i + 1].Density);
		}
	}
	
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_Descending, FPCGTestBaseClass, "Plugins.PCG.SortPoints.Descending", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_Descending::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::GenerateTestDataAndRunSort(EPCGSortMethod::Descending, true);

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	// compare point data
	const FPCGTaggedData& Output = Outputs[0];
	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Output.Data);
	if (OutPointData)
	{
		const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

		for (int i = 0; i < OutPoints.Num() - 1; ++i)
		{
			UTEST_TRUE(FString::Format(TEXT("{0} is greater than/equal to {1}"), { OutPoints[i].Density, OutPoints[i + 1].Density }), OutPoints[i].Density >= OutPoints[i + 1].Density);
		}
	}
	
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_SameValues, FPCGTestBaseClass, "Plugins.PCG.SortPoints.SameValues", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_SameValues::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::GenerateTestDataAndRunSort(EPCGSortMethod::Ascending, false);

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	// compare point data
	const FPCGTaggedData& Input = Inputs[0];
	const FPCGTaggedData& Output = Outputs[0];

	const UPCGPointData* InPointData = Cast<UPCGPointData>(Input.Data);
	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Output.Data);
	if (OutPointData && InPointData)
	{
		const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();
		const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

		UTEST_TRUE("Arrays have the same number of points:", InPoints.Num() == OutPoints.Num());

		for (int i = 0; i < InPoints.Num(); ++i)
		{
			//if they're in the same spots after sorting, they should have exactly the same properties across the board
			UTEST_EQUAL(FString::Format(TEXT("UnsortedArray[{0}].Seed is equal to SortedArray[{0}].Seed"), {i}), OutPoints[i].Seed, InPoints[i].Seed);
			UTEST_EQUAL(FString::Format(TEXT("UnsortedArray[{0}].Density is equal to SortedArray[{0}].Density"), {i}), OutPoints[i].Density, InPoints[i].Density);
			UTEST_EQUAL(FString::Format(TEXT("UnsortedArray[{0}].Transform is equal to SortedArray[{0}].Transform"), {i}), OutPoints[i].Transform, InPoints[i].Transform);
		}
	}

	return true;
}