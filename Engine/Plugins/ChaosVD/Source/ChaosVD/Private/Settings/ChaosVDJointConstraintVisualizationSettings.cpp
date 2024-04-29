// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDJointConstraintVisualizationSettings.h"

void UChaosVDJointConstraintsVisualizationSettings::SetJointsDataVisualizationFlags(EChaosVDJointsDataVisualizationFlags NewFlags)
{
	if (UChaosVDJointConstraintsVisualizationSettings* Settings = GetMutableDefault<UChaosVDJointConstraintsVisualizationSettings>())
	{
		Settings->GlobalJointsDataVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged();
	}
}

EChaosVDJointsDataVisualizationFlags UChaosVDJointConstraintsVisualizationSettings::GetJointsDataVisualizationFlags()
{
	if (UChaosVDJointConstraintsVisualizationSettings* Settings = GetMutableDefault<UChaosVDJointConstraintsVisualizationSettings>())
	{
		return Settings->GlobalJointsDataVisualizationFlags;
	}

	return EChaosVDJointsDataVisualizationFlags::None;
}

