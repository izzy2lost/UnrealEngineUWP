// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpModule.h"

#include "DisplayClusterWarpLog.h"
#include "DisplayClusterWarpStrings.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"

FDisplayClusterWarpModule::FDisplayClusterWarpModule()
{
	UE_LOG(LogDisplayClusterWarp, Verbose, TEXT("Warp module has been instantiated"));
}

FDisplayClusterWarpModule::~FDisplayClusterWarpModule()
{
	UE_LOG(LogDisplayClusterWarp, Verbose, TEXT("Warp module has been destroyed"));
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IModuleInterface
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterWarpModule::StartupModule()
{
	UE_LOG(LogDisplayClusterWarp, Verbose, TEXT("Warp module has been started"));
}

void FDisplayClusterWarpModule::ShutdownModule()
{
	UE_LOG(LogDisplayClusterWarp, Verbose, TEXT("Warp module has been shutdown"));
}

IMPLEMENT_MODULE(FDisplayClusterWarpModule, DisplayClusterWarp);
