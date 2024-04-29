// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSceneQueryVisualizationSettings.h"

void UChaosVDSceneQueriesVisualizationSettings::SetSceneQueryDataVisualizationFlags(EChaosVDSceneQueryVisualizationFlags NewFlags)
{
	if (UChaosVDSceneQueriesVisualizationSettings* Settings = GetMutableDefault<UChaosVDSceneQueriesVisualizationSettings>())
	{
		Settings->GlobalSceneQueriesVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged(Settings);
	}
}

EChaosVDSceneQueryVisualizationFlags UChaosVDSceneQueriesVisualizationSettings::GetSceneQueryDataVisualizationFlags()
{
	if (UChaosVDSceneQueriesVisualizationSettings* Settings = GetMutableDefault<UChaosVDSceneQueriesVisualizationSettings>())
	{
		return Settings->GlobalSceneQueriesVisualizationFlags;
	}

	return EChaosVDSceneQueryVisualizationFlags::None;
}
