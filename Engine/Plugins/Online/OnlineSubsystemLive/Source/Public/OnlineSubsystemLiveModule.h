// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "OnlineSubsystem.h"

/**
 * Online subsystem module class  (Live Implementation)
 * Code related to the loading of the Live module
 */
class FOnlineSubsystemLiveModule : public IModuleInterface
{
private:
	/** Class responsible for creating instance(s) of the subsystem */
	TUniquePtr<IOnlineFactory> LiveFactory;

public:
	FOnlineSubsystemLiveModule() = default;
	virtual ~FOnlineSubsystemLiveModule() = default;
	FOnlineSubsystemLiveModule(const FOnlineSubsystemLiveModule& Other) = delete;

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