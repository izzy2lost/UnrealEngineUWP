// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RuntimeGen/GenSources/PCGGenSourceBase.h"

class APCGWorldActor;
class UPCGGenSourceComponent;

class AController;
class AGameModeBase;
class APlayerController;
class UWorld;

/**
 * The runtime Generation Source Manager tracks generation sources in the world for use by the Runtime Generation Scheduler.
 */
class FPCGGenSourceManager
{
public:
	FPCGGenSourceManager(const UWorld* InWorld);
	~FPCGGenSourceManager();

	/** Creates the set of all GenerationSources in the level based on the contents of the GenSourceManager's world. */
	TSet<IPCGGenSourceBase*> GetGenSources(const APCGWorldActor* InPCGWorldActor) const;

	/** Adds a UPCGGenSource to be tracked by the GenSourceManager. */
	bool RegisterGenSource(IPCGGenSourceBase* InGenSource);

	/** Removes a UPCGGenSource from being tracked by the GenSourceManager. */
	bool UnregisterGenSource(const IPCGGenSourceBase* InGenSource);

protected:
	void OnGameModePostLogin(AGameModeBase* InGameMode, APlayerController* InPlayerController);
	void OnGameModePostLogout(AGameModeBase* InGameMode, AController* InController);

protected:
	TSet<IPCGGenSourceBase*> GenSources;

	const UWorld* World = nullptr;
};
