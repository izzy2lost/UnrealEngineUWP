// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCollisionVisualizationSettings.h"

void UChaosVDCollisionDataVisualizationSettings::SetDataVisualizationFlags(EChaosVDCollisionVisualizationFlags NewFlags)
{
	if (UChaosVDCollisionDataVisualizationSettings* Settings = GetMutableDefault<UChaosVDCollisionDataVisualizationSettings>())
	{
		Settings->CollisionDataVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged();
	}
}

EChaosVDCollisionVisualizationFlags UChaosVDCollisionDataVisualizationSettings::GetDataVisualizationFlags()
{
	if (UChaosVDCollisionDataVisualizationSettings* Settings = GetMutableDefault<UChaosVDCollisionDataVisualizationSettings>())
	{
		return Settings->CollisionDataVisualizationFlags ;
	}

	return EChaosVDCollisionVisualizationFlags::None;
}

