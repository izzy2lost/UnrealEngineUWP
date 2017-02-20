// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleInterface.h"

/**
 * Online subsystem module class  (Live Implementation)
 * Code related to the loading of the Live module
 */
class FOnlineSubsystemLiveModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactoryLive* LiveFactory;

public:

	FOnlineSubsystemLiveModule() :
		LiveFactory(NULL)
	{}

	virtual ~FOnlineSubsystemLiveModule() {}

	// IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
};

typedef TSharedPtr<FOnlineSubsystemLiveModule> FOnlineSubsystemLiveModulePtr;