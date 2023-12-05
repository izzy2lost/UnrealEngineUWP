// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlRequest.h"

#include "Control/Messages/ControlJsonUtilities.h"
#include "Control/Messages/Constants.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TGetServerInformationTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.GetServerInformation.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TGetServerInformationTest::RunTest(const FString& InParameters)
{
	FGetServerInformationRequest Request;

	TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetServerInformation);
	TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TKeepAliveRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.KeepAlive.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TKeepAliveRequestTest::RunTest(const FString& InParameters)
{        
    FKeepAliveRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GKeepAlive);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TStartSessionRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.StartSession.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TStartSessionRequestTest::RunTest(const FString& InParameters)
{
    FStartSessionRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStartSession);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TStopSessionRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.StopSession.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TStopSessionRequestTest::RunTest(const FString& InParameters)
{
    FStopSessionRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStopSession);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TSubscribeRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.Subscribe.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TSubscribeRequestTest::RunTest(const FString& InParameters)
{
    FSubscribeRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GSubscribe);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TUnsubscribeRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.Unsubscribe.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TUnsubscribeRequestTest::RunTest(const FString& InParameters)
{
    FUnsubscribeRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GUnsubscribe);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TGetStateRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.GetState.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TGetStateRequestTest::RunTest(const FString& InParameters)
{
    FGetStateRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetState);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TStartRecordingTakeRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.StartRecordingTake.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TStartRecordingTakeRequestTest::RunTest(const FString& InParameters)
{
	FString String = TEXT("{\
        \"slateName\": \"Slate\",\
        \"takeNumber\": 0,\
        \"subject\": \"Subject\",\
        \"scenario\": \"Scenario\",\
        \"tags\": [\"Tag1\", \"Tag2\", \"Tag3\"]\
    }");

	String.ConvertTabsToSpacesInline(2);
	String.RemoveSpacesInline();

	auto ConvertedString = StringCast<UTF8CHAR>(*String);

    FString SlateName = TEXT("Slate");
    FString Subject = TEXT("Subject");
    FString Scenario = TEXT("Scenario");
    TArray<FString> Tags = { TEXT("Tag1"), TEXT("Tag2"), TEXT("Tag3") };
    
    FStartRecordingTakeRequest Request(SlateName, 0, Subject, Scenario, Tags);

    TArray<uint8> Body;
    TestTrue(TEXT("CreateDataFromJson"), FJsonUtility::CreateUTF8DataFromJson(Request.GetBody(), Body));

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStartRecordingTake);

	TArray<uint8> BodyExpected(reinterpret_cast<const uint8*>(ConvertedString.Get()), ConvertedString.Length());
	TestEqual(TEXT("GetBody"), Body, BodyExpected);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TStopRecordingTakeRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.StopRecordingTake.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TStopRecordingTakeRequestTest::RunTest(const FString& InParameters)
{
	FStopRecordingTakeRequest Request;

	TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStopRecordingTake);
	TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TAbortRecordingTakeRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.AbortRecordingTake.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TAbortRecordingTakeRequestTest::RunTest(const FString& InParameters)
{
	FAbortRecordingTakeRequest Request;

	TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GAbortRecordingTake);
	TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TGetTakeListRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.GetTakeList.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TGetTakeListRequestTest::RunTest(const FString& InParameters)
{
    FGetTakeListRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetTakeList);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TGetTakeMetadataRequestTest, "Plugin.CaptureProtocolStack.Control.ControlRequest.GetTakeMetadata.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TGetTakeMetadataRequestTest::RunTest(const FString& InParameters)
{
	FString String = TEXT("{\
        \"names\": [\"TakeName1\", \"TakeName2\", \"TakeName3\"]\
    }");

	String.ConvertTabsToSpacesInline(2);
	String.RemoveSpacesInline();

    auto ConvertedString = StringCast<UTF8CHAR>(*String);

    TArray<FString> Takes = { TEXT("TakeName1"), TEXT("TakeName2"), TEXT("TakeName3") };
    FGetTakeMetadataRequest Request(Takes);

    TArray<uint8> Body;
    TestTrue(TEXT("CreateDataFromJson"), FJsonUtility::CreateUTF8DataFromJson(Request.GetBody(), Body));

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetTakeMetadata);

	TArray<uint8> BodyExpected(reinterpret_cast<const uint8*>(ConvertedString.Get()), ConvertedString.Length());
    TestEqual(TEXT("GetBody"), Body, BodyExpected);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS