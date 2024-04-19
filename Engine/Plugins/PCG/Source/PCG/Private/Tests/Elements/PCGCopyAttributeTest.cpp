// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Elements/Metadata/PCGMetadataElement.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Points_PropertyToProperty, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Points.PropertyToProperty", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Points_PropertyToAttribute, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Points.PropertyToAttribute", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Points_AttributeToProperty, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Points.AttributeToProperty", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Points_AttributeToAttribute, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Points.AttributeToAttribute", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Points_CopyingToItself, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Points.CopyingToItself", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Points_CopyingAllToItself, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Points.CopyingAllToItself", PCGTestsCommon::TestFlags)

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Params_SingleValue, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Params.SingleValue", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Params_MultiValue, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Params.MultiValue", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Params_CopyingToItself, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Params.CopyingToItself", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyAttributeTests_Params_CopyingAllToItself, FPCGTestBaseClass, "Plugins.PCG.CopyAttribute.Params.CopyingAllToItself", PCGTestsCommon::TestFlags)


namespace PCGCopyAttributeTests
{
	static const FName AttributeName = TEXT("Double");

	UPCGPointData* CreateInputPointData(FPCGContext* Context, const int NumPoints)
	{
		check(Context);

		UPCGPointData* NewPointData = NewObject<UPCGPointData>();
		NewPointData->SetFlags(RF_Transient);

		check(NewPointData && NewPointData->Metadata);

		// Create an attribute
		FPCGMetadataAttribute<double>* NewAttribute = NewPointData->Metadata->CreateAttribute<double>(AttributeName, /*DefaultValue=*/0.0, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false);

		TArray<FPCGPoint>& Points = NewPointData->GetMutablePoints();
		Points.SetNum(NumPoints);
		for (int i = 0; i < NumPoints; ++i)
		{
			// Store the index in the density
			Points[i].Density = i;

			 // And also in NewAttribute (offset by 5 to differientiate from density)
			NewPointData->Metadata->InitializeOnSet(Points[i].MetadataEntry);
			NewAttribute->SetValue(Points[i].MetadataEntry, i + 5);
		}

		FPCGTaggedData& InputData = Context->InputData.TaggedData.Emplace_GetRef();
		InputData.Data = NewPointData;
		InputData.Pin = PCGPinConstants::DefaultInputLabel;

		return NewPointData;
	}

	UPCGParamData* CreateInputParamData(FPCGContext* Context, const int NumEntries)
	{
		check(Context);

		UPCGParamData* NewParamData = NewObject<UPCGParamData>();
		NewParamData->SetFlags(RF_Transient);

		check(NewParamData && NewParamData->Metadata);

		// Create an attribute
		FPCGMetadataAttribute<double>* NewAttribute = NewParamData->Metadata->CreateAttribute<double>(AttributeName, /*DefaultValue=*/0.0, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false);

		for (int i = 0; i < NumEntries; ++i)
		{
			// Store the index offset by one in the attribute
			NewAttribute->SetValue(NewParamData->Metadata->AddEntry(), i + 1);
		}

		FPCGTaggedData& InputData = Context->InputData.TaggedData.Emplace_GetRef();
		InputData.Data = NewParamData;
		InputData.Pin = PCGPinConstants::DefaultInputLabel;

		return NewParamData;
	}
}

bool FPCGCopyAttributeTests_Points_PropertyToProperty::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;

	// Write Density in Position.X
	Settings->InputSource.SetPointProperty(EPCGPointProperties::Density);
	Settings->OutputTarget.Update(TEXT("$Position.X"));

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGCopyAttributeTests::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGPointData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGPointData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in output points", OutputData);
	UTEST_EQUAL("There is the right number of points in output", OutputData->GetPoints().Num(), NumOfPoints);

	check(OutputData);

	// Density was copied in the position correctly
	for (int i = 0; i < NumOfPoints; ++i)
	{
		const FPCGPoint& OutputPoint = OutputData->GetPoints()[i];
		UTEST_EQUAL(*FString::Printf(TEXT("Position.X has the same value as density for point %d"), i), OutputPoint.Transform.GetLocation().X, static_cast<double>(OutputPoint.Density));
	}

	return true;
}

bool FPCGCopyAttributeTests_Points_PropertyToAttribute::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;
	const FName OutputAttributeName = TEXT("OutputAttr");

	// Write Density in Attribute
	Settings->InputSource.SetPointProperty(EPCGPointProperties::Density);
	Settings->OutputTarget.SetAttributeName(OutputAttributeName);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGCopyAttributeTests::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGPointData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGPointData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in output points", OutputData);
	UTEST_EQUAL("There is the right number of points in output", OutputData->GetPoints().Num(), NumOfPoints);

	check(OutputData && OutputData->Metadata);
	const FPCGMetadataAttribute<double>* OutputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(OutputAttributeName);
	UTEST_NOT_NULL("Output Attribute exists in the output data", OutputAttribute);

	check(OutputAttribute);

	// Attribute was copied in the position correctly
	for (int i = 0; i < NumOfPoints; ++i)
	{
		const FPCGPoint& OutputPoint = OutputData->GetPoints()[i];
		UTEST_EQUAL(*FString::Printf(TEXT("Output attribute has the same value than density for point %d"), i), OutputAttribute->GetValueFromItemKey(OutputPoint.MetadataEntry), static_cast<double>(OutputPoint.Density));
	}

	return true;
}

bool FPCGCopyAttributeTests_Points_AttributeToProperty::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;

	// Write Attribute in Position.Y
	Settings->InputSource.SetAttributeName(PCGCopyAttributeTests::AttributeName);
	Settings->OutputTarget.Update(TEXT("$Position.Y"));

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGCopyAttributeTests::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGPointData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGPointData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in output points", OutputData);
	UTEST_EQUAL("There is the right number of points in output", OutputData->GetPoints().Num(), NumOfPoints);

	check(OutputData && OutputData->Metadata);
	const FPCGMetadataAttribute<double>* InputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(PCGCopyAttributeTests::AttributeName);
	UTEST_NOT_NULL("Input Attribute exists in the output data", InputAttribute);

	// Attribute value was copied in Position.Y correctly
	for (int i = 0; i < NumOfPoints; ++i)
	{
		const FPCGPoint& OutputPoint = OutputData->GetPoints()[i];
		UTEST_EQUAL(*FString::Printf(TEXT("Position.Y has the same value than the input attribute for point %d"), i), OutputPoint.Transform.GetLocation().Y, InputAttribute->GetValueFromItemKey(OutputPoint.MetadataEntry));
	}

	return true;
}

bool FPCGCopyAttributeTests_Points_AttributeToAttribute::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;
	const FName OutputAttributeName = TEXT("OutputAttr");

	// Write Input attribute to Output attribute
	Settings->InputSource.SetAttributeName(PCGCopyAttributeTests::AttributeName);
	Settings->OutputTarget.SetAttributeName(OutputAttributeName);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGCopyAttributeTests::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGPointData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGPointData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in output points", OutputData);
	UTEST_EQUAL("There is the right number of points in output", OutputData->GetPoints().Num(), NumOfPoints);

	check(OutputData && OutputData->Metadata);

	const FPCGMetadataAttribute<double>* InputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(PCGCopyAttributeTests::AttributeName);
	UTEST_NOT_NULL("Input Attribute exists in the output data", InputAttribute);
	const FPCGMetadataAttribute<double>* OutputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(OutputAttributeName);
	UTEST_NOT_NULL("Output Attribute exists in the output data", OutputAttribute);

	check(InputAttribute && OutputAttribute);

	UTEST_EQUAL("Output Attribute default value is the same as default value of Input Attribute", OutputAttribute->GetValueFromItemKey(PCGInvalidEntryKey), InputAttribute->GetValueFromItemKey(PCGInvalidEntryKey));

	// Input attribute was copied in the output attribute correctly
	for (int i = 0; i < NumOfPoints; ++i)
	{
		const FPCGPoint& OutputPoint = OutputData->GetPoints()[i];
		UTEST_EQUAL(*FString::Printf(TEXT("Output Attribute has the same value as Input Attribute for point %d"), i), OutputAttribute->GetValueFromItemKey(OutputPoint.MetadataEntry), InputAttribute->GetValueFromItemKey(OutputPoint.MetadataEntry));
	}

	return true;
}

bool FPCGCopyAttributeTests_Points_CopyingToItself::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;

	// Write Attribute in itself
	Settings->InputSource.SetAttributeName(PCGCopyAttributeTests::AttributeName);
	Settings->OutputTarget.SetAttributeName(PCGCopyAttributeTests::AttributeName);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGCopyAttributeTests::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGPointData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGPointData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in output points", OutputData);
	UTEST_EQUAL("It's the same data as input", OutputData, InputPointData);

	return true;
}

bool FPCGCopyAttributeTests_Points_CopyingAllToItself::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumOfPoints = 20;

	Settings->bCopyAllAttributes = true;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGPointData* InputPointData = PCGCopyAttributeTests::CreateInputPointData(Context.Get(), NumOfPoints);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGPointData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGPointData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a point data in output points", OutputData);
	UTEST_EQUAL("It's the same data as input", OutputData, InputPointData);

	return true;
}

bool FPCGCopyAttributeTests_Params_SingleValue::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	const FName OutputAttributeName = TEXT("OutputAttr");

	// Write Input attribute to Output attribute
	Settings->InputSource.SetAttributeName(PCGCopyAttributeTests::AttributeName);
	Settings->OutputTarget.SetAttributeName(OutputAttributeName);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGParamData* InputParamData = PCGCopyAttributeTests::CreateInputParamData(Context.Get(), 1);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGParamData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGParamData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a param data in output points", OutputData);
	UTEST_EQUAL("There is the right number of entries in output metadata", OutputData->Metadata->GetItemCountForChild(), 1);

	check(OutputData && OutputData->Metadata);

	const FPCGMetadataAttribute<double>* InputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(PCGCopyAttributeTests::AttributeName);
	UTEST_NOT_NULL("Input Attribute exists in the output data", InputAttribute);
	const FPCGMetadataAttribute<double>* OutputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(OutputAttributeName);
	UTEST_NOT_NULL("Output Attribute exists in the output data", OutputAttribute);

	check(InputAttribute && OutputAttribute);

	UTEST_EQUAL("Output Attribute default value is the same as first entry of Input Attribute", OutputAttribute->GetValueFromItemKey(PCGInvalidEntryKey), InputAttribute->GetValueFromItemKey(PCGMetadataEntryKey(0)));
	UTEST_EQUAL("Output Attribute has the same value as Input Attribute for entry 0", OutputAttribute->GetValueFromItemKey(PCGMetadataEntryKey(0)), InputAttribute->GetValueFromItemKey(PCGMetadataEntryKey(0)));

	return true;
}

bool FPCGCopyAttributeTests_Params_MultiValue::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	constexpr int NumEntries = 20;
	const FName OutputAttributeName = TEXT("OutputAttr");

	// Write Input attribute to Output attribute
	Settings->InputSource.SetAttributeName(PCGCopyAttributeTests::AttributeName);
	Settings->OutputTarget.SetAttributeName(OutputAttributeName);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGParamData* InputParamData = PCGCopyAttributeTests::CreateInputParamData(Context.Get(), NumEntries);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGParamData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGParamData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a param data in output points", OutputData);
	UTEST_EQUAL("There is the right number of entries in output metadata", OutputData->Metadata->GetItemCountForChild(), NumEntries);

	check(OutputData && OutputData->Metadata);

	const FPCGMetadataAttribute<double>* InputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(PCGCopyAttributeTests::AttributeName);
	UTEST_NOT_NULL("Input Attribute exists in the output data", InputAttribute);
	const FPCGMetadataAttribute<double>* OutputAttribute = OutputData->Metadata->GetConstTypedAttribute<double>(OutputAttributeName);
	UTEST_NOT_NULL("Output Attribute exists in the output data", OutputAttribute);

	check(InputAttribute && OutputAttribute);

	UTEST_EQUAL("Output Attribute default value is the same as default value of Input Attribute", OutputAttribute->GetValueFromItemKey(PCGInvalidEntryKey), InputAttribute->GetValueFromItemKey(PCGInvalidEntryKey));

	// Input attribute was copied in the output attribute correctly
	for (int i = 0; i < NumEntries; ++i)
	{
		UTEST_EQUAL(*FString::Printf(TEXT("Output Attribute has the same value as Input Attribute for entry %d"), i), OutputAttribute->GetValueFromItemKey(PCGMetadataEntryKey(i)), InputAttribute->GetValueFromItemKey(PCGMetadataEntryKey(i)));
	}

	return true;
}

bool FPCGCopyAttributeTests_Params_CopyingToItself::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	// Write Input attribute to Output attribute
	Settings->InputSource.SetAttributeName(PCGCopyAttributeTests::AttributeName);
	Settings->OutputTarget.SetAttributeName(PCGCopyAttributeTests::AttributeName);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGParamData* InputParamData = PCGCopyAttributeTests::CreateInputParamData(Context.Get(), 1);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGParamData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGParamData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a param data in output points", OutputData);
	UTEST_EQUAL("It's the same data as input", OutputData, InputParamData);

	return true;
}

bool FPCGCopyAttributeTests_Params_CopyingAllToItself::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataOperationSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataOperationSettings>(TestData);
	check(Settings);

	Settings->bCopyAllAttributes = true;

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
	const UPCGParamData* InputParamData = PCGCopyAttributeTests::CreateInputParamData(Context.Get(), 1);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	while (!TestElement->Execute(Context.Get())) {}

	TArray<FPCGTaggedData> OutputTagged = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
	const UPCGParamData* OutputData = OutputTagged.Num() == 1 ? Cast<const UPCGParamData>(OutputTagged[0].Data) : nullptr;

	UTEST_NOT_NULL("There is a param data in output points", OutputData);
	UTEST_EQUAL("It's the same data as input", OutputData, InputParamData);

	return true;
}


#endif // WITH_EDITOR
