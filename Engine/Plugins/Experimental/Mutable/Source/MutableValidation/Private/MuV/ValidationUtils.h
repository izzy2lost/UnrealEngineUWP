// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * Prepare the asset registry so we can later use it to search assets. It is required by Mutable to compile.
 */
void PrepareAssetRegistry();


/**
 * Hold the thread for the time specified while ticking the engine.
 * @param ToWaitSeconds The time in seconds we want to hold the execution of the thread
 */
void Wait(const double ToWaitSeconds);


/**
 * Logs some configuration data related to how mutable will compile and then generate instances. We do this so we can later
 * Isolate tests using different configurations.
 * @note Add new logs each time you add a way to change the configuration of the test from the .xml testing file
 */
void LogMutableSettings();
