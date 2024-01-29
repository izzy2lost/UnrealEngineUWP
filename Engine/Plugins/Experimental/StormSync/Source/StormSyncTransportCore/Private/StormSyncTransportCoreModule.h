// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStormSyncTransportCoreModule.h"

class FStormSyncTransportCoreModule : public IStormSyncTransportCoreModule
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
