// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h" // required for ue programs

IMPLEMENT_APPLICATION(AutoRTFMTests, "AutoRTFMTests");

#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch_amalgamated.cpp"

int main(int ArgC, const char* ArgV[]) {
	Catch::Session Session;

	bool NoRetry = false;
	bool RetryNestedToo = false;

	Session.cli(Session.cli()
		| Catch::Clara::Opt(NoRetry)["--no-retry"]
		| Catch::Clara::Opt(RetryNestedToo)["--retry-nested-too"]);

	{
		const int Result = Session.applyCommandLine(ArgC, ArgV);

		if (0 != Result)
		{
			return Result;
		}
	}

	if (RetryNestedToo)
	{
		AutoRTFM::ForTheRuntime::SetRetryTransaction(AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState::RetryNestedToo);
	}
	else if (NoRetry)
	{
		AutoRTFM::ForTheRuntime::SetRetryTransaction(AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState::NoRetry);
	}
	else
	{
		// Otherwise default to just retrying the parent transaction.
		AutoRTFM::ForTheRuntime::SetRetryTransaction(AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState::RetryNonNested);
	}

	const TCHAR* CommandLine = TEXT("-Multiprocess");
	GEngineLoop.PreInit(CommandLine);
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Enable AutoRTFM.
	AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::EAutoRTFMEnabledState::AutoRTFM_Enabled);

	// We don't want to trigger ensure's on abort because we are going to test that.
	AutoRTFM::ForTheRuntime::SetEnsureOnAbortByLanguage(false);

	const int Result = Session.run();

	FPlatformMisc::RequestExit(false);

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return Result;
}
