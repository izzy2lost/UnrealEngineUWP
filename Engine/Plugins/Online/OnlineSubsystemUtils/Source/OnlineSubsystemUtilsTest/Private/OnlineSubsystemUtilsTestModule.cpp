// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "OnlineBeaconUnitTestSocketSubsystem.h"

/**
 * Online subsystem utils test module class
 * Test module used to implement automation testing to be handled within editor builds.
 */
class FOnlineSubsystemUtilsTestModule : public IModuleInterface
{
public:

	FOnlineSubsystemUtilsTestModule()
	{
	}

	virtual ~FOnlineSubsystemUtilsTestModule()
	{
	}

	// IModuleInterface
	virtual void StartupModule() override
	{
		FString InitError;
		SocketSubsystem = MakeShared<FOnlineBeaconUnitTestSocketSubsystem>();
		ensure(SocketSubsystem->Init(InitError));
	}

	virtual void ShutdownModule() override
	{
		SocketSubsystem->Shutdown();
		SocketSubsystem.Reset();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	TSharedPtr<FOnlineBeaconUnitTestSocketSubsystem> SocketSubsystem;
};

IMPLEMENT_MODULE(FOnlineSubsystemUtilsTestModule, OnlineSubsystemUtilsTest);
