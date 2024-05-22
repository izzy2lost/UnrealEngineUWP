// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSceneQueryVisualizationSettings.h"

void UChaosVDSceneQueriesVisualizationSettings::SetDataVisualizationFlags(EChaosVDSceneQueryVisualizationFlags NewFlags)
{
	if (UChaosVDSceneQueriesVisualizationSettings* Settings = GetMutableDefault<UChaosVDSceneQueriesVisualizationSettings>())
	{
		Settings->GlobalSceneQueriesVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged();
	}
}

EChaosVDSceneQueryVisualizationFlags UChaosVDSceneQueriesVisualizationSettings::GetDataVisualizationFlags()
{
	if (UChaosVDSceneQueriesVisualizationSettings* Settings = GetMutableDefault<UChaosVDSceneQueriesVisualizationSettings>())
	{
		return Settings->GlobalSceneQueriesVisualizationFlags;
	}

	return EChaosVDSceneQueryVisualizationFlags::None;
}
