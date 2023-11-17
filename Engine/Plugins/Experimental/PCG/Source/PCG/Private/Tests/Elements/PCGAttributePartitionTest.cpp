// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"

#include "Elements/Metadata/PCGMetadataPartition.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_Points, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.Points", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_AttributeSet, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.AttributeSet", PCGTestsCommon::TestFlags)

bool FPCGAttributePartition_Points::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	Settings->PartitionAttributeSource.SetPointProperty(EPCGPointProperties::Density);

	UPCGPointData* InputPointData = NewObject<UPCGPointData>();
	TArray<FPCGPoint>& Points = InputPointData->GetMutablePoints();
	Points.Reserve(100);
	for (int32 i = 0; i < 100; ++i)
	{
		FPCGPoint& Point = Points.Emplace_GetRef();
		Point.Density = (i % 10) / 10.0f;
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputPointData;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 10 outputs", Context->OutputData.TaggedData.Num(), 10);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGPointData* OutputPointData = Cast<const UPCGPointData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a point data"), i), OutputPointData);

		const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has 10 points"), i), OutputPoints.Num(), 10);

		float TempValue = -1.0f;
		bool bFirstPoint = true;
		bool bAllEquals = true;
		for (const FPCGPoint& Point : OutputPoints)
		{
			if (bFirstPoint)
			{
				TempValue = Point.Density;
				bFirstPoint = false;
			}
			else
			{
				bAllEquals &= TempValue == Point.Density;
			}
		}

		UTEST_TRUE(*FString::Printf(TEXT("Output points for output %d have all the same density"), i), bAllEquals);
	}

	return true;
}

bool FPCGAttributePartition_AttributeSet::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	const FName InputAttributeName = TEXT("Double");
	Settings->PartitionAttributeSource.SetAttributeName(InputAttributeName);

	UPCGParamData* InputParam = NewObject<UPCGParamData>();
	FPCGMetadataAttribute<double>* Attribute = InputParam->Metadata->CreateAttribute<double>(InputAttributeName, 0.0, true, false);
	for (int32 i = 0; i < 100; ++i)
	{
		Attribute->SetValue(InputParam->Metadata->AddEntry(), (i % 10) / 10.0);
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputParam;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 10 outputs", Context->OutputData.TaggedData.Num(), 10);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGParamData* OutputParamData = Cast<const UPCGParamData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a param data"), i), OutputParamData);

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has 10 entries"), i), OutputParamData->Metadata->GetLocalItemCount(), 10ll);
		const FPCGMetadataAttribute<double>* OutAttribute = OutputParamData->Metadata->GetConstTypedAttribute<double>(InputAttributeName);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d has the 'Double' attribute"), i), OutAttribute);


		double TempValue = -1.0;
		bool bFirstItem = true;
		bool bAllEquals = true;
		for (PCGMetadataEntryKey Key = 0; Key < OutputParamData->Metadata->GetLocalItemCount(); ++Key)
		{
			if (bFirstItem)
			{
				TempValue = OutAttribute->GetValueFromItemKey(Key);
				bFirstItem = false;
			}
			else
			{
				bAllEquals &= TempValue == OutAttribute->GetValueFromItemKey(Key);
			}
		}

		UTEST_TRUE(*FString::Printf(TEXT("Output values for output %d are all the same"), i), bAllEquals);
	}

	return true;
}