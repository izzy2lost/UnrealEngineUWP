// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcUtils.h"
#include "Logging.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "epic_rtc/core/platform.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::PixelStreaming2
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicRtcPlatformCreateTest, "System.Plugins.PixelStreaming2.FEpicRtcPlatformCreateTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FEpicRtcPlatformCreateTest::RunTest(const FString& Parameters)
	{
		EpicRtcErrorCode Result;

		TRefCountPtr<EpicRtcPlatformInterface> Platform;
		Result = GetOrCreatePlatform({}, Platform.GetInitReference());
		if (!ValidatePlatform(Platform, Result, { EpicRtcErrorCode::Ok, EpicRtcErrorCode::FoundExistingPlatform }, 1))
		{
			return false;
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicRtcPlatformLifetimeTest, "System.Plugins.PixelStreaming2.FEpicRtcPlatformLifetimeTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FEpicRtcPlatformLifetimeTest::RunTest(const FString& Parameters)
	{
		EpicRtcPlatformInterface* PlatformPtr;
		uint32 CreationCount;
		{
			TRefCountPtr<EpicRtcPlatformInterface> Platform;

			EpicRtcErrorCode Result = GetOrCreatePlatform({}, Platform.GetInitReference());
			if (!ValidatePlatform(Platform, Result, { EpicRtcErrorCode::Ok, EpicRtcErrorCode::FoundExistingPlatform }, 1))
			{
				return false;
			}
			CreationCount = Platform->Count();

			PlatformPtr = Platform.GetReference();
			PlatformPtr->AddRef();	// Platform will go out of scope so will have sane count
		}

		// Because the platform is is stored internal to EpicRtc and is shared between PS, EOSSDK, Platform will still exists so will be at least 1
		// Count should be lower than CreationCount
		if (uint32 Count = PlatformPtr->Release(); Count != CreationCount-1 && Count != 0)
		{
			UE_LOG(LogPixelStreaming2, Error, TEXT("Failed to validate platform. Platform has invalid reference count. Expected (%d), Actual (%d)"), CreationCount-1, Count);
			return false;
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicRtcPlatformGetTest, "System.Plugins.PixelStreaming2.FEpicRtcPlatformGetTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FEpicRtcPlatformGetTest::RunTest(const FString& Parameters)
	{
		EpicRtcErrorCode Result;

		TRefCountPtr<EpicRtcPlatformInterface> Platform;
		Result = GetOrCreatePlatform({}, Platform.GetInitReference());
		if (!ValidatePlatform(Platform, Result, { EpicRtcErrorCode::Ok, EpicRtcErrorCode::FoundExistingPlatform }, 1))
		{
			return false;
		}

		TRefCountPtr<EpicRtcPlatformInterface> OtherPlatform;

		Result = GetOrCreatePlatform({}, OtherPlatform.GetInitReference());
		if (!ValidatePlatform(OtherPlatform, Result, { EpicRtcErrorCode::FoundExistingPlatform }, 2))
		{
			return false;
		}

		if (Platform.GetReference() != OtherPlatform.GetReference())
		{
			UE_LOG(LogPixelStreaming2, Error, TEXT("Expected Platform and OtherPlatform to reference the same pointer"));
			return false;
		}

		return true;
	}
} // namespace UE::PixelStreaming2

#endif
