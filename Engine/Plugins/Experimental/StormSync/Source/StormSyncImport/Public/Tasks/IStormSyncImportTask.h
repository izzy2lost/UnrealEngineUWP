// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Interface for tasks that need delayed execution */
class IStormSyncImportSubsystemTask
{
public:
	virtual ~IStormSyncImportSubsystemTask()
	{
	}

	virtual void Run() = 0;
};
