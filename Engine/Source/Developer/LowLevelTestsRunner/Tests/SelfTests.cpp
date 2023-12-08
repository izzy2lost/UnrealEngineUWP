// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "TestHarness.h"

TEST_CASE("LowLevelTestsRunner::SelfTests::Skip")
{
	SKIP("SKIP is expected to work all the time.");
	FAIL("Test wasn't skipped.");
}

#endif