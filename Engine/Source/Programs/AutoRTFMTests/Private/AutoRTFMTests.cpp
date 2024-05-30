// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h" // required for ue programs

IMPLEMENT_APPLICATION(AutoRTFMTests, "AutoRTFMTests");

#include "catch_amalgamated.cpp"

class SetupListener final : public Catch::EventListenerBase
{
public:
	using Catch::EventListenerBase::EventListenerBase;

	void testRunStarting(const Catch::TestRunInfo&) override
	{
		GEngineLoop.PreInit(0, nullptr);
		FModuleManager::Get().StartProcessingNewlyLoadedObjects();

		// Enable all Verse code to run under AutoRTFM (shouldn't affect our tests here, but better safe than sorry).
		AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::EAutoRTFMEnabledState::AutoRTFM_EnabledForAllVerse);

		// We don't want to trigger ensure's on abort because we are going to test that.
		AutoRTFM::ForTheRuntime::SetEnsureOnAbortByLanguage(false);
	}

	void testRunEnded(const Catch::TestRunStats&) override
	{
		FPlatformMisc::RequestExit(false);

		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	}
};

CATCH_REGISTER_LISTENER(SetupListener)
