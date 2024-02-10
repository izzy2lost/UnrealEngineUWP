// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS


#include "TestHarness.h"

#if WITH_EDITORONLY_DATA
#include "Experimental/ZenServerInterface.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace UE
{

TEST_CASE("Zen::ZenServerInterface", "[Zen][Basic]")
{
	using namespace UE::Zen;
	uint16 DefaultTestPort = 8559;
	uint16 CurrentTestPort = DefaultTestPort + 1;
	FString DefaultArgs = TEXT("--http asio");
	FString DataPathRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineSavedDir(), "ZenServerInterfaceUnitTest"));
	FString DefaultDataPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(DataPathRoot, "Default"));
	IFileManager::Get().DeleteDirectory(*DataPathRoot, false, true);

	SECTION("Basic AutoLaunch and Shutdown")
	{
		for (int Iteration = 0; Iteration < 2; ++Iteration)
		{
			FServiceSettings ZenTestServiceSettings;
			FServiceAutoLaunchSettings& ZenTestAutoLaunchSettings = ZenTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
			ZenTestAutoLaunchSettings.DataPath = DefaultDataPath;
			ZenTestAutoLaunchSettings.ExtraArgs = DefaultArgs;
			ZenTestAutoLaunchSettings.DesiredPort = DefaultTestPort;

			{
				FScopeZenService ScopeZenService(MoveTemp(ZenTestServiceSettings));
				FZenServiceInstance& ZenInstance = ScopeZenService.GetInstance();
				uint16 AutoLaunchedPort = ZenInstance.GetAutoLaunchedPort();
				uint16 DetectedPort = 0;
				CHECK(ZenInstance.IsServiceReady());

				CHECK(IsLocalServiceRunning(*DefaultDataPath, &DetectedPort));
				CHECK(DetectedPort == AutoLaunchedPort);
				CHECK(StopLocalService(*DefaultDataPath));
				CHECK(!IsLocalServiceRunning(*DefaultDataPath));
			}
		}
	}

	SECTION("Overlapping AutoLaunch and Shutdown")
	{
		for (int Iteration = 0; Iteration < 3; ++Iteration)
		{
			FServiceSettings ZenTestServiceSettings;
			FServiceAutoLaunchSettings& ZenTestAutoLaunchSettings = ZenTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
			ZenTestAutoLaunchSettings.DataPath = DefaultDataPath;
			ZenTestAutoLaunchSettings.ExtraArgs = DefaultArgs;
			ZenTestAutoLaunchSettings.DesiredPort = DefaultTestPort;

			{
				FScopeZenService ScopeZenService(MoveTemp(ZenTestServiceSettings));
				FZenServiceInstance& ZenInstance = ScopeZenService.GetInstance();
				uint16 AutoLaunchedPort = ZenInstance.GetAutoLaunchedPort();
				uint16 DetectedPort = 0;
				CHECK(ZenInstance.IsServiceReady());

				CHECK(IsLocalServiceRunning(*DefaultDataPath, &DetectedPort));
				CHECK(DetectedPort == AutoLaunchedPort);
			}
		}
		CHECK(StopLocalService(*DefaultDataPath));
		CHECK(!IsLocalServiceRunning(*DefaultDataPath));
	}

	SECTION("Server moving between separate datapaths with same port")
	{
		FString LastDataPath;
		{
			TArray<FScopeZenService> ConcurrentServices;
			for (int Iteration = 0; Iteration < 3; ++Iteration)
			{
				FString DataPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(DataPathRoot, FString::Printf(TEXT("Instance%d"), Iteration)));
				LastDataPath = DataPath;
				FServiceSettings ZenTestServiceSettings;
				FServiceAutoLaunchSettings& ZenTestAutoLaunchSettings = ZenTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
				ZenTestAutoLaunchSettings.DataPath = DataPath;
				ZenTestAutoLaunchSettings.ExtraArgs = DefaultArgs;
				ZenTestAutoLaunchSettings.DesiredPort = DefaultTestPort;

				{
					FScopeZenService& ScopeZenService = ConcurrentServices.Emplace_GetRef(MoveTemp(ZenTestServiceSettings));
					FZenServiceInstance& ZenInstance = ScopeZenService.GetInstance();
					uint16 AutoLaunchedPort = ZenInstance.GetAutoLaunchedPort();
					uint16 DetectedPort = 0;
					CHECK(ZenInstance.IsServiceReady());

					CHECK(IsLocalServiceRunning(*DataPath, &DetectedPort));
					CHECK(DetectedPort == AutoLaunchedPort);

					for (int PastIteration = 0; PastIteration < Iteration; ++PastIteration)
					{
						// By starting a new service on the same port as an existing one, we expect that the existing one
						// must be torn down.  Check that here.
						const FServiceAutoLaunchSettings& PastZenTestAutoLaunchSettings = ConcurrentServices[PastIteration].GetInstance()
							.GetServiceSettings().SettingsVariant.Get<FServiceAutoLaunchSettings>();
						CHECK(!IsLocalServiceRunning(*PastZenTestAutoLaunchSettings.DataPath));
					}
				}
			}
		}
		CHECK(StopLocalService(*LastDataPath));
		CHECK(!IsLocalServiceRunning(*LastDataPath));
	}

	SECTION("Overlapping AutoLaunch and Shutdown with DataPath shared and differing args")
	{
		for (int Iteration = 0; Iteration < 3; ++Iteration)
		{
			FServiceSettings ZenTestServiceSettings;
			FServiceAutoLaunchSettings& ZenTestAutoLaunchSettings = ZenTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
			ZenTestAutoLaunchSettings.DataPath = DefaultDataPath;
			ZenTestAutoLaunchSettings.ExtraArgs = *WriteToString<128>(DefaultArgs, TEXT(" --gc-interval-seconds "), (Iteration+1)*1000);
			ZenTestAutoLaunchSettings.DesiredPort = DefaultTestPort;

			{
				FScopeZenService ScopeZenService(MoveTemp(ZenTestServiceSettings));
				FZenServiceInstance& ZenInstance = ScopeZenService.GetInstance();
				uint16 AutoLaunchedPort = ZenInstance.GetAutoLaunchedPort();
				uint16 DetectedPort = 0;
				CHECK(ZenInstance.IsServiceReady());

				CHECK(IsLocalServiceRunning(*DefaultDataPath, &DetectedPort));
				CHECK(DetectedPort == AutoLaunchedPort);
			}
		}
		CHECK(StopLocalService(*DefaultDataPath));
		CHECK(!IsLocalServiceRunning(*DefaultDataPath));
	}
}

} // UE
#endif // WITH_EDITORONLY_DATA

#endif // WITH_LOW_LEVEL_TESTS
