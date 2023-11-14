// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h" // required for ue programs

IMPLEMENT_APPLICATION(AutoRTFMTests, "AutoRTFMTests");

#include "catch_amalgamated.cpp"

class SetupListener : public Catch::EventListenerBase {
public:
	using Catch::EventListenerBase::EventListenerBase;

	void testRunStarting(Catch::TestRunInfo const&) override
	{
		AutoRTFM::SetAutoRTFMRuntime(true);
	}
};

CATCH_REGISTER_LISTENER(SetupListener)
