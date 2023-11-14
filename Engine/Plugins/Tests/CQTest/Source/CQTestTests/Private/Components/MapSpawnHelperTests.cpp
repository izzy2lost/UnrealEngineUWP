// Copyright Epic Games, Inc. All Rights Reserved.

#define ENABLE_MAPSPAWNER_TEST WITH_EDITOR && (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

#if ENABLE_MAPSPAWNER_TEST

#include "CQTest.h"
#include "Components/MapTestSpawner.h"
#include "GameFramework/Pawn.h"

TEST_CLASS(MapSpawnHelperTests, "TestFramework.CQTest.Map")
{
	TUniquePtr<FMapTestSpawner> Spawner;

	BEFORE_EACH() {
		Spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
		ASSERT_THAT(IsNotNull(Spawner));

		Spawner->AddWaitUntilLoadedCommand(TestRunner);
		TestCommandBuilder.Do([&]() {
			// Because we're creating a level for this test, we will also want to populate the level with a Pawn object that can be then tested against
			Spawner->SpawnActor<APawn>();
		});
	}

	TEST_METHOD(MapSpawner_FindsPlayerSpawn)
	{
		TestCommandBuilder.Do([&]() {
			APawn* Player = Spawner->FindFirstPlayerPawn();
			ASSERT_THAT(IsNotNull(Player));
		});
	}
};

#endif // ENABLE_MAPSPAWNER_TEST