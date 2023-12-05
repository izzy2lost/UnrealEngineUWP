// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlUpdate.h"

#include "Control/Messages/ControlJsonUtilities.h"
#include "Control/Messages/Constants.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TSessionStoppedUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.SessionStopped.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TSessionStoppedUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body;

    FSessionStopped Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GSessionStopped);
    TestTrue(TEXT("Parse"), Update.Parse(Body).HasValue());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TTakeAddedUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.TakeAdded.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TTakeAddedUpdateTest::RunTest(const FString& InParameters)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(UE::CPS::Properties::GName, TEXT("TakeName"));

	FTakeAddedUpdate Update;

	TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GTakeAdded);

	TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
	TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
	TestEqual(TEXT("Parse"), Update.GetTakeName(), TEXT("TakeName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TTakeRemovedUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.TakeRemoved.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TTakeRemovedUpdateTest::RunTest(const FString& InParameters)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(UE::CPS::Properties::GName, TEXT("TakeName"));

	FTakeRemovedUpdate Update;

	TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GTakeRemoved);

	TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
	TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
	TestEqual(TEXT("Parse"), Update.GetTakeName(), TEXT("TakeName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TTakeUpdatedUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.TakeUpdated.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TTakeUpdatedUpdateTest::RunTest(const FString& InParameters)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(UE::CPS::Properties::GName, TEXT("TakeName"));

	FTakeUpdatedUpdate Update;

	TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GTakeUpdated);

	TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
	TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
	TestEqual(TEXT("Parse"), Update.GetTakeName(), TEXT("TakeName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TRecordingStatusUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.RecordingStatus.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TRecordingStatusUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetBoolField(UE::CPS::Properties::GIsRecording, true);

    FRecordingStatusUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GRecordingStatus);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
    TestEqual(TEXT("Parse"), Update.IsRecording(), true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiskCapacityUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.DiskCapacity.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiskCapacityUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetNumberField(UE::CPS::Properties::GTotal, 1024);
    Body->SetNumberField(UE::CPS::Properties::GRemaining, 512);

    FDiskCapacityUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GDiskCapacity);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
    TestEqual(TEXT("Parse"), Update.GetTotal(), (uint64)1024);
    TestEqual(TEXT("Parse"), Update.GetRemaining(), (uint64)512);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TBatteryPercentageUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.BatteryPercentage.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TBatteryPercentageUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetNumberField(UE::CPS::Properties::GLevel, 100.0);

    FBatteryPercentageUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GBattery);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
    TestEqual(TEXT("Parse"), Update.GetLevel(), (float) 100.0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TThermalStateUpdateTest, "Plugin.CaptureProtocolStack.Control.ControlUpdate.ThermalState.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TThermalStateUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();

    Body->SetStringField(UE::CPS::Properties::GState, TEXT("nominal"));

    FThermalStateUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GThermalState);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.HasValue());
    TestEqual(TEXT("Parse"), Update.GetState(), FThermalStateUpdate::EState::Nominal);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS