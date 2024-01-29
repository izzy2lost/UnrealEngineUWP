// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IStormSyncImportModule.h"

/** Storm Sync Import module implementation */
class FStormSyncImportModule : public IStormSyncImportModule
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
