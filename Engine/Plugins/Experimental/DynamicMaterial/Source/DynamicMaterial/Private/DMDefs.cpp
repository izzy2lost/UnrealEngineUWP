// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMDefs.h"

int32 FDMUpdateGuard::GuardCount = 0;
uint32 FDMInitializationGuard::GuardCount = 0;

bool FDMInitializationGuard::IsInitializing()
{
	return GuardCount > 0;
}

FDMInitializationGuard::FDMInitializationGuard()
{
	// Used the struct name to make it clear it's a static variable.
	++FDMInitializationGuard::GuardCount;
}

FDMInitializationGuard::~FDMInitializationGuard()
{
	if (FDMInitializationGuard::GuardCount > 0)
	{
		--FDMInitializationGuard::GuardCount;
	}
}
