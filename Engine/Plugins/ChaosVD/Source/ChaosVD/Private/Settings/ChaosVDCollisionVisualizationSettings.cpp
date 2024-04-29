// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCollisionVisualizationSettings.h"

void UChaosVDCollisionDataVisualizationSettings::SetCollisionDataVisualizationFlags(EChaosVDCollisionVisualizationFlags NewFlags)
{
	if (UChaosVDCollisionDataVisualizationSettings* Settings = GetMutableDefault<UChaosVDCollisionDataVisualizationSettings>())
	{
		Settings->CollisionDataVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged(Settings);
	}
}

EChaosVDCollisionVisualizationFlags UChaosVDCollisionDataVisualizationSettings::GetCollisionDataVisualizationFlags()
{
	if (UChaosVDCollisionDataVisualizationSettings* Settings = GetMutableDefault<UChaosVDCollisionDataVisualizationSettings>())
	{
		return Settings->CollisionDataVisualizationFlags ;
	}

	return EChaosVDCollisionVisualizationFlags::None;
}

