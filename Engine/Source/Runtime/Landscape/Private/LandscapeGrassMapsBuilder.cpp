// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeGrassMapsBuilder.h"
#include "Misc/ScopedSlowTask.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeGrassWeightExporter.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "Landscape"

extern int32 GGrassEnable;


#if WITH_EDITOR

FLandscapeGrassMapsBuilder::FLandscapeGrassMapsBuilder(UWorld* InOwner)
	: World(InOwner)
	, OutdatedGrassMapCount(0)
	, GrassMapsLastCheckTime(0)
{}

void FLandscapeGrassMapsBuilder::Build()
{
	if (World)
	{
		int32 Count = GetOutdatedGrassMapCount();
		FScopedSlowTask SlowTask(static_cast<float>(Count), (LOCTEXT("GrassMaps_BuildGrassMaps", "Building Grass maps")));
		SlowTask.MakeDialog();

		for (TActorIterator<ALandscapeProxy> ProxyIt(World); ProxyIt; ++ProxyIt)
		{
			ProxyIt->BuildGrassMaps(&SlowTask);
		}
	}
}

int32 FLandscapeGrassMapsBuilder::GetOutdatedGrassMapCount(bool bInForceUpdate) const
{
	if (World)
	{
		bool bUpdate = bInForceUpdate || GLandscapeEditModeActive;
		if (!bUpdate)
		{
			double GrassMapsTimeNow = FPlatformTime::Seconds();
			// Recheck every 20 secs to handle the case where levels may have been Streamed in/out
			if ((GrassMapsTimeNow - GrassMapsLastCheckTime) > 20)
			{
				GrassMapsLastCheckTime = GrassMapsTimeNow;
				bUpdate = true;
			}
		}

		if (bUpdate)
		{
			OutdatedGrassMapCount = 0;
			for (TActorIterator<ALandscapeProxy> ProxyIt(World); ProxyIt; ++ProxyIt)
			{
				OutdatedGrassMapCount += ProxyIt->GetOutdatedGrassMapCount();
			}
		}
	}
	return OutdatedGrassMapCount;
}

#endif // WITH_EDITOR