// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"

#include "Elements/PCGRandomChoice.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGRandomChoiceTest_Fixed, FPCGTestBaseClass, "Plugins.PCG.RandomChoice,Fixed", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGRandomChoiceTest_Ratio, FPCGTestBaseClass, "Plugins.PCG.RandomChoice.Ratio", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGRandomChoiceTest_SelectNone, FPCGTestBaseClass, "Plugins.PCG.RandomChoice.SelectNone", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGRandomChoiceTest_SelectAll, FPCGTestBaseClass, "Plugins.PCG.RandomChoice.SelectAll", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGRandomChoiceTest_NoDiscard, FPCGTestBaseClass, "Plugins.PCG.RandomChoice.NoDiscard", PCGTestsCommon::TestFlags)

namespace PCGRandomChoiceTest
{
	UPCGPointData* CreateInputPointData(FPCGContext* Context, const int NumPoints)
	{
		check(Context);

		UPCGPointData* NewPointData = NewObject<UPCGPointData>();
		NewPointData->SetFlags(RF_Transient);
		TArray<FPCGPoint>& Points = NewPointData->GetMutablePoints();
		Points.SetNum(NumPoints);
		for (int i = 0; i < NumPoints; ++i)
		{
			// Store the index in the density
			Points[i].Density = i;
		}

		FPCGTaggedData& InputData = Context->InputData.TaggedData.Emplace_GetRef();
		InputData.Data = NewPointData;
		InputData.Pin = PCGPinConstants::DefaultInputLabel;

		return NewPointData;
	}

	bool VerifyAllPointsThere(const int NumPoints, const UPCGPointData* ChosenPointData, const UPCGPointData* DiscardedPointData)
	{
		check(ChosenPointData && DiscardedPointData);
		TSet<int> IndexesSeen;

		auto Check = [&IndexesSeen](const UPCGPointData* PointData)
		{
			const TArray<FPCGPoint>& Points = PointData->GetPoints();
			for (int i = 0; i < Points.Num(); ++i)
			{
				const FPCGPoint& Point = Points[i];
				const int Index = static_cast<int>(Point.Density);
				if (IndexesSeen.Contains(Index))
				{
					return false;
				}

				IndexesSeen.Add(Index);

				// It needs to be stable so density should be ascending.
				if (i > 0 && Points[i].Density < Points[i - 1].Density)
				{
					return false;
				}
			}

			return true;
		};

		if (!Check(ChosenPointData))
		{
			return false;
		}

		if (!Check(DiscardedPointData))
		{
			return false;
		}

		return IndexesSeen.Num() == NumPoints;
	}
}

bool FPCGRandomChoiceTest_Fixed::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGRandomChoiceSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRandomChoiceSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;
	constexpr int ExpectedNumElementsChosen = 7;
	constexpr int ExpectedNumElementsDiscarded = 13;

	Settings->bFixedMode = true;
	Settings->FixedNumber = ExpectedNumElementsChosen;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGRandomChoiceTest::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> ChosenOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::ChosenPointsLabel);
	TArray<FPCGTaggedData> DiscardedOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::DiscardedPointsLabel);

	const UPCGPointData* ChosenOutputData = ChosenOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(ChosenOutputTagged[0].Data) : nullptr;
	const UPCGPointData* DiscardedOutputData = DiscardedOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(DiscardedOutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in chosen points", ChosenOutputData);
	UTEST_NOT_NULL("There is a point data in discarded points", DiscardedOutputData);

	UTEST_EQUAL("There is the right number of points in chosen", ChosenOutputData->GetPoints().Num(), ExpectedNumElementsChosen);
	UTEST_EQUAL("There is the right number of points in discarded", DiscardedOutputData->GetPoints().Num(), ExpectedNumElementsDiscarded);

	UTEST_TRUE("All points are there and in the right order", PCGRandomChoiceTest::VerifyAllPointsThere(NumOfPoints, ChosenOutputData, DiscardedOutputData));

	return true;
}

bool FPCGRandomChoiceTest_Ratio::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGRandomChoiceSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRandomChoiceSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;
	constexpr int ExpectedNumElementsChosen = 5;
	constexpr int ExpectedNumElementsDiscarded = 15;

	Settings->bFixedMode = false;
	Settings->Ratio = 0.25f;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGRandomChoiceTest::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> ChosenOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::ChosenPointsLabel);
	TArray<FPCGTaggedData> DiscardedOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::DiscardedPointsLabel);

	const UPCGPointData* ChosenOutputData = ChosenOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(ChosenOutputTagged[0].Data) : nullptr;
	const UPCGPointData* DiscardedOutputData = DiscardedOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(DiscardedOutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in chosen points", ChosenOutputData);
	UTEST_NOT_NULL("There is a point data in discarded points", DiscardedOutputData);

	UTEST_EQUAL("There is the right number of points in chosen", ChosenOutputData->GetPoints().Num(), ExpectedNumElementsChosen);
	UTEST_EQUAL("There is the right number of points in discarded", DiscardedOutputData->GetPoints().Num(), ExpectedNumElementsDiscarded);

	UTEST_TRUE("All points are there and in the right order", PCGRandomChoiceTest::VerifyAllPointsThere(NumOfPoints, ChosenOutputData, DiscardedOutputData));

	return true;
}

bool FPCGRandomChoiceTest_SelectNone::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGRandomChoiceSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRandomChoiceSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;

	Settings->bFixedMode = true;
	Settings->FixedNumber = 0;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGRandomChoiceTest::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> ChosenOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::ChosenPointsLabel);
	TArray<FPCGTaggedData> DiscardedOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::DiscardedPointsLabel);

	const UPCGPointData* ChosenOutputData = ChosenOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(ChosenOutputTagged[0].Data) : nullptr;
	const UPCGPointData* DiscardedOutputData = DiscardedOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(DiscardedOutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in chosen points", ChosenOutputData);
	UTEST_NOT_NULL("There is a point data in discarded points", DiscardedOutputData);

	UTEST_EQUAL("Chosen points is empty", ChosenOutputData->GetPoints().Num(), 0);
	UTEST_EQUAL("Discarded points is the input data", DiscardedOutputData, InputPointData);

	return true;
}

bool FPCGRandomChoiceTest_SelectAll::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGRandomChoiceSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRandomChoiceSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;

	Settings->bFixedMode = false;
	Settings->Ratio = 1.0f;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGRandomChoiceTest::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> ChosenOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::ChosenPointsLabel);
	TArray<FPCGTaggedData> DiscardedOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::DiscardedPointsLabel);

	const UPCGPointData* ChosenOutputData = ChosenOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(ChosenOutputTagged[0].Data) : nullptr;
	const UPCGPointData* DiscardedOutputData = DiscardedOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(DiscardedOutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in chosen points", ChosenOutputData);
	UTEST_NOT_NULL("There is a point data in discarded points", DiscardedOutputData);

	UTEST_EQUAL("Chosen points is the input data", ChosenOutputData, InputPointData);
	UTEST_EQUAL("Discarded points is empty", DiscardedOutputData->GetPoints().Num(), 0);

	return true;
}

bool FPCGRandomChoiceTest_NoDiscard::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGRandomChoiceSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRandomChoiceSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;
	constexpr int ExpectedNumElementsChosen = 2;

	Settings->bFixedMode = false;
	Settings->Ratio = 0.1f;
	Settings->bOutputDiscardedPoints = false;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGRandomChoiceTest::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> ChosenOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::ChosenPointsLabel);
	TArray<FPCGTaggedData> DiscardedOutputTagged = Context->OutputData.GetInputsByPin(PCGRandomChoiceConstants::DiscardedPointsLabel);

	const UPCGPointData* ChosenOutputData = ChosenOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(ChosenOutputTagged[0].Data) : nullptr;
	const UPCGPointData* DiscardedOutputData = DiscardedOutputTagged.Num() == 1 ? Cast<const UPCGPointData>(DiscardedOutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in chosen points", ChosenOutputData);
	UTEST_NULL("There is no point data in discarded points", DiscardedOutputData);

	UTEST_EQUAL("There is the right number of points in chosen", ChosenOutputData->GetPoints().Num(), ExpectedNumElementsChosen);

	return true;
}
#endif // WITH_EDITOR
