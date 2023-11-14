// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SpawnHelper.h"

#include "Commands/TestCommandBuilder.h"

/// Class for spawning Actors in a named Map/Level
struct CQTEST_API FMapTestSpawner : public FSpawnHelper
{
	/**
	 * Construct the MapTestSpawner.
	 *
	 * @param MapDirectory - The directory which the map resides in.
	 * @param MapName - Name of the map.
	 */
	FMapTestSpawner(const FString& MapDirectory, const FString& MapName)
		: MapDirectory(MapDirectory)
		, MapName(MapName)
	{
	}

	/**
	 * Creates an instance of the MapTestSpawner with a temporary level ready for use.
	 * 
	 * @param InCommandBuilder - Test Command Builder used to assist with setup.
	 * @return unique instance of the FMapTestSpawner, nullptr otherwise
	 */
	static TUniquePtr<FMapTestSpawner> CreateFromTempLevel(FTestCommandBuilder& InCommandBuilder);

	/**
	 * Loads the map specified from the MapDirectory and MapName to be prepared for the test.
	 *
	 * @param TestRunner - TestRunner used to send the latent command needed for map preparations.
	 */
	void AddWaitUntilLoadedCommand(FAutomationTestBase* TestRunner);

	/**
	 * Finds the first pawn in the given map.
	 */
	APawn* FindFirstPlayerPawn();

protected:
    virtual UWorld* CreateWorld() override;

private:
	FString MapDirectory;
	FString MapName;
	UWorld* PieWorld{ nullptr };
};