// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Details/Media/DisplayClusterConfiguratorMediaUtils.h"


FDisplayClusterConfiguratorMediaUtils& FDisplayClusterConfiguratorMediaUtils::Get()
{
	static FDisplayClusterConfiguratorMediaUtils Instance;
	return Instance;
}
