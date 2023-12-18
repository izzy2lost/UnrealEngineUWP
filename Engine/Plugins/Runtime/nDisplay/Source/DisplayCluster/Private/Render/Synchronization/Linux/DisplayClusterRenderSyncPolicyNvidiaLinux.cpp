// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNvidia.h"


FDisplayClusterRenderSyncPolicyNvidia::FDisplayClusterRenderSyncPolicyNvidia(const TMap<FString, FString>& Parameters)
	: FDisplayClusterRenderSyncPolicyBase(Parameters)
{
}

FDisplayClusterRenderSyncPolicyNvidia::~FDisplayClusterRenderSyncPolicyNvidia()
{
}

bool FDisplayClusterRenderSyncPolicyNvidia::Initialize()
{
	return true;
}

bool FDisplayClusterRenderSyncPolicyNvidia::InitializeNvidiaSwapLock()
{
	return true;
}

void FDisplayClusterRenderSyncPolicyNvidia::SetMaximumFrameLatency(uint8 FrameLatency)
{
	// not implemented yet
}

bool FDisplayClusterRenderSyncPolicyNvidia::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	// Wait unless the frame is rendered
	WaitForFrameCompletion();
	// Sync on the network barrier
	SyncOnBarrier();
	// Ask engine to present the frame
	return true;
}
