// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformChunkInstall.h"

DEFINE_LOG_CATEGORY(LogChunkInstaller)

void FGenericPlatformChunkInstall::DoNamedChunkCompleteCallbacks( const FName NamedChunk, EChunkLocation::Type Location, bool bHasSucceeded )
{
	check(IsInGameThread());

	bool bIsInstalled = (Location == EChunkLocation::LocalFast) || (Location == EChunkLocation::LocalSlow);

	if (NamedChunkCompleteDelegate.IsBound())
	{
		FNamedChunkCompleteCallbackParam Param;
		Param.NamedChunk = NamedChunk;
		Param.Location = Location;
		Param.bIsInstalled = bIsInstalled;
		Param.bHasSucceeded = bHasSucceeded;

		NamedChunkCompleteDelegate.Broadcast(Param);
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (bIsInstalled && NamedChunkInstallDelegate.IsBound())
	{
		NamedChunkInstallDelegate.Broadcast(NamedChunk, bHasSucceeded);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
