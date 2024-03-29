// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * State graph manager for AGameSession::RegisterServer.
 */

#include "StateGraphManager.h"
#include "Subsystems/WorldSubsystem.h"

#include "RegisterServerManager.generated.h"

namespace UE::RegisterServer
{
STATEGRAPHMANAGER_API extern const FName StateGraphName;
} // UE::RegisterServer

/** Subsystem manager that other modules and subsystems can depend on to add RegisterServer state graph delegates with. */
UCLASS()
class STATEGRAPHMANAGER_API URegisterServerManager : public UWorldSubsystem, public UE::FStateGraphManager
{
	GENERATED_BODY()

public:
	virtual FName GetStateGraphName() const override
	{
		return UE::RegisterServer::StateGraphName;
	}
};
